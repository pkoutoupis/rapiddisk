/*********************************************************************************
 ** Copyright (c) 2011-2015 Petros Koutoupis
 ** All rights reserved.
 **
 ** filename: rdsk.c
 ** description: RapidDisk is an enhanced Linux RAM disk module to dynamically
 **	 create, remove, and resize RAM drives. RapidDisk supports both volatile
 **	 and non-volatile memory.
 ** created: 1Jun11, petros@petroskoutoupis.com
 **
 ** This file is licensed under GPLv2.
 **
 ********************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#include <linux/fs.h>
#else
#include <linux/buffer_head.h>
#endif
#include <linux/hdreg.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/errno.h>
#include <linux/radix-tree.h>
#include <asm/io.h>

#define VERSION_STR		"3.2"
#define RDPREFIX		"rxd"
#define BYTES_PER_SECTOR	512
#define MAX_RxDISKS		128
#define DEFAULT_MAX_SECTS	127
#define DEFAULT_REQUESTS	128
#define GENERIC_ERROR		-1

#define FREE_BATCH		16
#define SECTOR_SHIFT		9
#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		(1 << PAGE_SECTORS_SHIFT)

#define NON_VOLATILE_MEMORY	0
#define VOLATILE_MEMORY		1

/* ioctls */
#define INVALID_CDQUERY_IOCTL	0x5331
#define RXD_GET_STATS		0x0529

static DEFINE_MUTEX(sysfs_mutex);
static DEFINE_MUTEX(rxioctl_mutex);

struct rdsk_device {
	int num; 
	bool volatile_memory;
	struct request_queue *rdsk_queue;
	struct gendisk *rdsk_disk;
	struct list_head rdsk_list;
	unsigned long long max_blk_alloc;	/* rdsk: to keep track of highest sector write	*/
	unsigned long long start_addr;		/* rdsk-nv: start of physical address		*/
	unsigned long long end_addr;		/* rdsk-nv: end of physical address		*/
	unsigned long long size;
	unsigned long error_cnt;
	spinlock_t rdsk_lock;
	struct radix_tree_root rdsk_pages;
};

static int rdsk_ma_no = 0, rxcnt = 0; /* no. of attached devices */
static int max_rxcnt = MAX_RxDISKS;
static int max_sectors = DEFAULT_MAX_SECTS, nr_requests = DEFAULT_REQUESTS;
static LIST_HEAD(rdsk_devices);
static struct kobject *rdsk_kobj;

module_param(max_rxcnt, int, S_IRUGO);
MODULE_PARM_DESC(max_rxcnt, " Total RAM Disk devices available for use. (Default = 128)");
module_param(max_sectors, int, S_IRUGO);
MODULE_PARM_DESC(max_sectors, " Maximum sectors (in KB) for the request queue. (Default = 127)");
module_param(nr_requests, int, S_IRUGO);
MODULE_PARM_DESC(nr_requests, " Number of requests at a given time for the request queue. (Default = 128)");

static int rdsk_do_bvec(struct rdsk_device *, struct page *, unsigned int, unsigned int, int, sector_t);
static int rdsk_ioctl(struct block_device *, fmode_t, unsigned int, unsigned long);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
static void rdsk_make_request(struct request_queue *, struct bio *);
#else
static int rdsk_make_request(struct request_queue *, struct bio *);
#endif
static int attach_device(int, int); /* disk num, disk size */
static int attach_nv_device(int, unsigned long long, unsigned long long); /* disk num, start / end addr */
static int detach_device(int);	  /* disk num */
static int resize_device(int, int); /* disk num, disk size */
static ssize_t mgmt_show(struct kobject *, struct kobj_attribute *, char *);
static ssize_t mgmt_store(struct kobject *, struct kobj_attribute *, const char *, size_t);


static ssize_t mgmt_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len;
	struct rdsk_device *rdsk;

	len = sprintf(buf, "RapidDisk (rxdsk) %s\n\nMaximum Number of Attachable Devices: %d\nNumber of "
		"Attached Devices: %d\nMax Sectors (KB): %d\nNumber of Requests: %d\n\n",
		VERSION_STR, max_rxcnt, rxcnt, max_sectors, nr_requests);
	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list){
		if (rdsk->volatile_memory == VOLATILE_MEMORY){
			len += sprintf(buf + len, "rxd%d\tSize: %llu MBs\tErrors: %lu\n",
				rdsk->num, (rdsk->size / 1024 / 1024), rdsk->error_cnt);
		}else{
			len += sprintf(buf + len, "rxd%d\tSize: %llu MBs\tErrors: %lu\tStart Address: %llu\tEnd Address: %llu\n",
				rdsk->num, (rdsk->size / 1024 / 1024), rdsk->error_cnt,
				rdsk->start_addr, rdsk->end_addr);
		}
	}
	return len;
}

static ssize_t mgmt_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buffer, size_t count)
{
	int num, size, err = (int)count;
	char *ptr, *buf;
	unsigned long long start_addr, end_addr;

	if (!buffer || count > PAGE_SIZE)
		return -EINVAL;

	mutex_lock(&sysfs_mutex);
	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto write_sysfs_error;
	}
	strcpy(buf, buffer);

	if (!strncmp("rxdsk attach ", buffer, 13)) {
		ptr = buf + 13;
		num = simple_strtoul(ptr, &ptr, 0);
		size = simple_strtoul(ptr + 1, &ptr, 0);

		if (attach_device(num, size) != 0) {
			pr_err("%s: Unable to attach rxd%d\n", RDPREFIX, num);
			err = -EINVAL;
		}
	} else if (!strncmp("rxdsk attach-nv ", buffer, 16)) {
		/* "rdsk attach-nv num start end" */
		ptr = buf + 16;
		num = simple_strtoul(ptr, &ptr, 0);
		start_addr = simple_strtoull(ptr + 1, &ptr, 0);
		end_addr = simple_strtoull(ptr + 1, &ptr, 0);

		if (attach_nv_device(num, start_addr, end_addr) != 0) {
			pr_err("%s: Unable to attach rxd%d\n", RDPREFIX, num);
			err = -EINVAL;
		}
	} else if (!strncmp("rxdsk detach ", buffer, 13)) {
		ptr = buf + 13;
		num = simple_strtoul(ptr, &ptr, 0);
		if (detach_device(num) != 0) {
			pr_err("%s: Unable to detach rxd%d\n", RDPREFIX, num);
			err = -EINVAL;
		}
	} else if (!strncmp("rxdsk resize ", buffer, 13)) {
		ptr = buf + 13;
		num = simple_strtoul(ptr, &ptr, 0);
		size = simple_strtoul(ptr + 1, &ptr, 0);

		if (resize_device(num, size) != 0) {
			pr_err("%s: Unable to resize rxd%d\n", RDPREFIX, num);
			err = -EINVAL;
		}
	} else {
		pr_err("%s: Unsupported command: %s\n", RDPREFIX, buffer);
		err = -EINVAL;
	}

	free_page((unsigned long)buf);
write_sysfs_error:
	mutex_unlock(&sysfs_mutex);
	return err;
}

static struct kobj_attribute mgmt_attribute =
	__ATTR(mgmt, 0664, mgmt_show, mgmt_store);

static struct attribute *attrs[] = {
	&mgmt_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct page *rdsk_lookup_page(struct rdsk_device *rdsk, sector_t sector)
{
	pgoff_t idx;
	struct page *page;

	rcu_read_lock();
	idx = sector >> PAGE_SECTORS_SHIFT; /* sector to page index */
	page = radix_tree_lookup(&rdsk->rdsk_pages, idx);
	rcu_read_unlock();

	BUG_ON(page && page->index != idx);

	return page;
}

static struct page *rdsk_insert_page(struct rdsk_device *rdsk, sector_t sector)
{
	pgoff_t idx;
	struct page *page;
	gfp_t gfp_flags;

	page = rdsk_lookup_page(rdsk, sector);
	if (page)
		return page;

	/*
	 * Must use NOIO because we don't want to recurse back into the
	 * block or filesystem layers from page reclaim.
	 *
	 * Cannot support XIP and highmem, because our ->direct_access
	 * routine for XIP must return memory that is always addressable.
	 * If XIP was reworked to use pfns and kmap throughout, this
	 * restriction might be able to be lifted.
	 */
	gfp_flags = GFP_NOIO | __GFP_ZERO;
#ifndef CONFIG_BLK_DEV_XIP
	gfp_flags |= __GFP_HIGHMEM;
#endif
	page = alloc_page(gfp_flags);
	if(!page)
		return NULL;

	if(radix_tree_preload(GFP_NOIO)){
		__free_page(page);
		return NULL;
	}

	spin_lock(&rdsk->rdsk_lock);
	idx = sector >> PAGE_SECTORS_SHIFT;
	if(radix_tree_insert(&rdsk->rdsk_pages, idx, page)){
		__free_page(page);
		page = radix_tree_lookup(&rdsk->rdsk_pages, idx);
		BUG_ON(!page);
		BUG_ON(page->index != idx);
	}else
		page->index = idx;
	spin_unlock(&rdsk->rdsk_lock);

	radix_tree_preload_end();

	return page;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static void rdsk_zero_page(struct rdsk_device *rdsk, sector_t sector)
{
	struct page *page;

	page = rdsk_lookup_page(rdsk, sector);
	if (page)
		clear_highpage(page);
}
#endif

static void rdsk_free_pages(struct rdsk_device *rdsk)
{
	unsigned long pos = 0;
	struct page *pages[FREE_BATCH];
	int nr_pages;

	do{
		int i;
		nr_pages = radix_tree_gang_lookup(&rdsk->rdsk_pages, (void **)pages, pos, FREE_BATCH);

		for(i = 0; i < nr_pages; i++){
			void *ret;

			BUG_ON(pages[i]->index < pos);
			pos = pages[i]->index;
			ret = radix_tree_delete(&rdsk->rdsk_pages, pos);
			BUG_ON(!ret || ret != pages[i]);
			__free_page(pages[i]);
		}
		pos++;
	}while(nr_pages == FREE_BATCH);
}

static int copy_to_rdsk_setup(struct rdsk_device *rdsk, sector_t sector, size_t n)
{
	unsigned int offset = (sector & (PAGE_SECTORS - 1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	if(!rdsk_insert_page(rdsk, sector))
		return -ENOMEM;
	if(copy < n){
		sector += copy >> SECTOR_SHIFT;
		if (!rdsk_insert_page(rdsk, sector))
			return -ENOMEM;
	}
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static void discard_from_rdsk(struct rdsk_device *rdsk, sector_t sector, size_t n)
{
	while (n >= PAGE_SIZE) {
		rdsk_zero_page(rdsk, sector);
		sector += PAGE_SIZE >> SECTOR_SHIFT;
		n -= PAGE_SIZE;
	}
}
#endif

static void copy_to_rdsk(struct rdsk_device *rdsk, const void *src, sector_t sector, size_t n)
{
	struct page *page;
	void *dst;
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = rdsk_lookup_page(rdsk, sector);
	BUG_ON(!page);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	dst = kmap_atomic(page);
#else
	dst = kmap_atomic(page, KM_USER1);
#endif
	memcpy(dst + offset, src, copy);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	kunmap_atomic(dst);
#else
	kunmap_atomic(dst, KM_USER1);
#endif

	if(copy < n){
		src += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;
		page = rdsk_lookup_page(rdsk, sector);
		BUG_ON(!page);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		dst = kmap_atomic(page);
#else
		dst = kmap_atomic(page, KM_USER1);
#endif
		memcpy(dst, src, copy);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		kunmap_atomic(dst);
#else
		kunmap_atomic(dst, KM_USER1);
#endif
	}

	if((sector + (n / BYTES_PER_SECTOR)) > rdsk->max_blk_alloc){
		rdsk->max_blk_alloc = (sector + (n / BYTES_PER_SECTOR));
	}
}

static void copy_from_rdsk(void *dst, struct rdsk_device *rdsk, sector_t sector, size_t n)
{
	struct page *page;
	void *src;
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = rdsk_lookup_page(rdsk, sector);

	if(page){
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		src = kmap_atomic(page);
#else
		src = kmap_atomic(page, KM_USER1);
#endif
		memcpy(dst, src + offset, copy);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		kunmap_atomic(src);
#else
		kunmap_atomic(src, KM_USER1);
#endif
	}else
		memset(dst, 0, copy);

	if(copy < n){
		dst += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;
		page = rdsk_lookup_page(rdsk, sector);
		if(page){
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
			src = kmap_atomic(page);
#else
			src = kmap_atomic(page, KM_USER1);
#endif
			memcpy(dst, src, copy);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
			kunmap_atomic(src);
#else
			kunmap_atomic(src, KM_USER1);
#endif
		}else
			memset(dst, 0, copy);
	}
}

static int
rdsk_do_bvec(struct rdsk_device *rdsk, struct page *page, unsigned int len,
		unsigned int off, int rw, sector_t sector){

	void *mem;
	int err = 0;
	void __iomem *vaddr = NULL;
	resource_size_t phys_addr = (rdsk->start_addr + (sector * BYTES_PER_SECTOR));

	if(rdsk->volatile_memory == VOLATILE_MEMORY){
		if(rw != READ){
			err = copy_to_rdsk_setup(rdsk, sector, len);
			if(err) goto out;
		}
	}else{
		if(((sector * BYTES_PER_SECTOR) + len) > rdsk->size){
			pr_err("%s: Beyond rxd%d boundary (offset: %llu len: %u).\n", RDPREFIX,
				rdsk->num, (unsigned long long)phys_addr, len);
			return -EFAULT;
		}

		if(!(vaddr = ioremap_nocache(phys_addr, len))){
			pr_err("%s: Unable to map memory at address %llu of size %lu\n",
				RDPREFIX, (unsigned long long)phys_addr, (unsigned long)len);
			return -EFAULT;
		}
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	mem = kmap_atomic(page);
#else
	mem = kmap_atomic(page, KM_USER0);
#endif
	if(rw == READ){
		if(rdsk->volatile_memory == VOLATILE_MEMORY){
			copy_from_rdsk(mem + off, rdsk, sector, len);
			flush_dcache_page(page);
		}else{
			memcpy(mem, vaddr, len);
		}
	}else{
		if(rdsk->volatile_memory == VOLATILE_MEMORY){
			flush_dcache_page(page);
			copy_to_rdsk(rdsk, mem + off, sector, len);
		}else{
			memcpy(vaddr, mem, len);
		}
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	kunmap_atomic(mem);
#else
	kunmap_atomic(mem, KM_USER0);
#endif
	if(rdsk->volatile_memory == NON_VOLATILE_MEMORY) iounmap(vaddr);
out:
	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
static void
#else
static int
#endif
rdsk_make_request(struct request_queue *q, struct bio *bio)
{
	struct block_device *bdev = bio->bi_bdev;
	struct rdsk_device *rdsk = bdev->bd_disk->private_data;
	int rw;
	sector_t sector;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	struct bio_vec bvec;
	struct bvec_iter iter;
#else
	struct bio_vec *bvec;
	int i;
#endif
	int err = -EIO;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	sector = bio->bi_iter.bi_sector;
#else
	sector = bio->bi_sector;
#endif
	if((sector + bio_sectors(bio)) > get_capacity(bdev->bd_disk))
		goto out;

	err = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
		discard_from_rdsk(rdsk, sector, bio->bi_iter.bi_size);
#else
		discard_from_rdsk(rdsk, sector, bio->bi_size);
#endif
		goto out;
	}
#endif
	rw = bio_rw(bio);
	if (rw == READA) rw = READ;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	bio_for_each_segment(bvec, bio, iter){
		unsigned int len = bvec.bv_len;
		err = rdsk_do_bvec(rdsk, bvec.bv_page, len, bvec.bv_offset, rw, sector);
#else
	bio_for_each_segment(bvec, bio, i){
		unsigned int len = bvec->bv_len;
		err = rdsk_do_bvec(rdsk, bvec->bv_page, len, bvec->bv_offset, rw, sector);
#endif
		if(err){
			rdsk->error_cnt++;
			break;
		}
		sector += len >> SECTOR_SHIFT;
	}

out:
	set_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_endio(bio, err);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	return 0;
#endif
}

static int rdsk_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	loff_t size;
	int error = 0;
	struct rdsk_device *rdsk = bdev->bd_disk->private_data;

	switch (cmd) {
		case BLKGETSIZE: {
			size = bdev->bd_inode->i_size;
			if ((size >> 9) > ~0UL)
				return -EFBIG;
			return copy_to_user ((void __user *)arg, &size, sizeof(size)) ? -EFAULT : 0;
		}
		case BLKGETSIZE64: {
			return copy_to_user ((void __user *)arg, &bdev->bd_inode->i_size, sizeof(bdev->bd_inode->i_size)) ? -EFAULT : 0;
		}
	case BLKFLSBUF: {
			if (rdsk->volatile_memory == NON_VOLATILE_MEMORY)
				return 0;
			/* We are killing the RAM disk data. */
			mutex_lock(&rxioctl_mutex);
			mutex_lock(&bdev->bd_mutex);
			error = -EBUSY;
			if (bdev->bd_openers <= 1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
				kill_bdev(bdev);
#else
				invalidate_bh_lrus();
				truncate_inode_pages(bdev->bd_inode->i_mapping, 0);
#endif
				rdsk_free_pages(rdsk);
				error = 0;
			}
			mutex_unlock(&bdev->bd_mutex);
			mutex_unlock(&rxioctl_mutex);
			return error;
	}
		case INVALID_CDQUERY_IOCTL: {
			return -EINVAL;
		}
		case RXD_GET_STATS: {
			return copy_to_user ((void __user *)arg, &rdsk->max_blk_alloc, sizeof(rdsk->max_blk_alloc)) ? -EFAULT : 0;
		}
		case BLKPBSZGET: 
		case BLKBSZGET: 
		case BLKSSZGET: {
			size = BYTES_PER_SECTOR;
			return copy_to_user ((void __user *)arg, &size, sizeof(size)) ? -EFAULT : 0;
		}
	}

	pr_warn("%s: 0x%x invalid ioctl.\n", RDPREFIX, cmd);
	return -ENOTTY;		/* unknown command */
} 

static const struct block_device_operations rdsk_fops = {
	.owner = THIS_MODULE,
	.ioctl = rdsk_ioctl,
};

static int attach_device(int num, int size)
{
	struct rdsk_device *rdsk, *rxtmp;
	struct gendisk *disk;

	if(rxcnt > max_rxcnt){
		pr_warn("%s: reached maximum number of attached disks. unable to attach more.\n", RDPREFIX);
		goto out;
	}

	list_for_each_entry(rxtmp, &rdsk_devices, rdsk_list){
		if(rxtmp->num == num){
			pr_warn("%s: rdsk device %d already exists.\n", RDPREFIX, num);
			goto out;
		} 
	}

	rdsk = kzalloc(sizeof(*rdsk), GFP_KERNEL);
	if(!rdsk) goto out;
	rdsk->num = num;
	rdsk->error_cnt = 0;
	rdsk->volatile_memory = VOLATILE_MEMORY;
	rdsk->max_blk_alloc = 0;
	rdsk->end_addr = rdsk->size = (size * BYTES_PER_SECTOR);
	rdsk->start_addr = 0;
	spin_lock_init(&rdsk->rdsk_lock);
	INIT_RADIX_TREE(&rdsk->rdsk_pages, GFP_ATOMIC);

	rdsk->rdsk_queue = blk_alloc_queue(GFP_KERNEL);
	if(!rdsk->rdsk_queue) goto out_free_dev;
	blk_queue_make_request(rdsk->rdsk_queue, rdsk_make_request);
	blk_queue_logical_block_size (rdsk->rdsk_queue, BYTES_PER_SECTOR);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	blk_queue_flush(rdsk->rdsk_queue, REQ_FLUSH);
#else
	blk_queue_ordered(rdsk->rdsk_queue, QUEUE_ORDERED_TAG, NULL);
#endif

	rdsk->rdsk_queue->limits.max_sectors = (max_sectors * 2);
	rdsk->rdsk_queue->nr_requests = nr_requests;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	rdsk->rdsk_queue->limits.discard_granularity = PAGE_SIZE;
	rdsk->rdsk_queue->limits.discard_zeroes_data = 1;
	rdsk->rdsk_queue->limits.max_discard_sectors = UINT_MAX;
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, rdsk->rdsk_queue);
#endif

	if (!(disk = rdsk->rdsk_disk = alloc_disk (1))) goto out_free_queue;
	disk->major = rdsk_ma_no;
	disk->first_minor = num;
	disk->fops = &rdsk_fops;
	disk->private_data = rdsk;
	disk->queue = rdsk->rdsk_queue;
	disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "%s%d", RDPREFIX, num);
	set_capacity(disk, size);

	add_disk(rdsk->rdsk_disk);
	list_add_tail(&rdsk->rdsk_list, &rdsk_devices);
	rxcnt++;
	pr_info("%s: Attached rxd%d of %llu bytes in size.\n", RDPREFIX, num,
		(unsigned long long)(size * BYTES_PER_SECTOR));
	return 0;

out_free_queue:
	blk_cleanup_queue(rdsk->rdsk_queue); 
out_free_dev:
	kfree(rdsk);
out:
	return GENERIC_ERROR;
}

static int attach_nv_device(int num, unsigned long long start_addr, unsigned long long end_addr)
{
	struct rdsk_device *rdsk, *rxtmp;
	struct gendisk *disk;
	unsigned long size = ((end_addr - start_addr) / BYTES_PER_SECTOR);

	if(rxcnt > max_rxcnt){
		pr_warn("%s: reached maximum number of attached disks. unable to attach more.\n", RDPREFIX);
		goto out_nv;
	}

	list_for_each_entry(rxtmp, &rdsk_devices, rdsk_list){
		if(rxtmp->num == num){
			pr_warn("%s: rdsk device %d already exists.\n", RDPREFIX, num);
			goto out_nv;
		}
	}
	if(!request_mem_region(start_addr, (end_addr - start_addr), "RapidDisk-NV")) {
		pr_err("%s: Failed to request memory region (address: %llu size: %llu).\n",
			RDPREFIX, start_addr, (end_addr - start_addr));
		goto out_nv;
	}

	rdsk = kzalloc(sizeof(*rdsk), GFP_KERNEL);
	if(!rdsk) goto out_nv;
	rdsk->num = num;
	rdsk->error_cnt = 0;
	rdsk->start_addr = start_addr;
	rdsk->end_addr = end_addr;
	rdsk->size = end_addr - start_addr;
	rdsk->volatile_memory = NON_VOLATILE_MEMORY;
	rdsk->max_blk_alloc = (rdsk->size / BYTES_PER_SECTOR);

	/* TODO: check if it crosses boundaries with another rdsk */
	spin_lock_init(&rdsk->rdsk_lock);

	rdsk->rdsk_queue = blk_alloc_queue(GFP_KERNEL);
	if(!rdsk->rdsk_queue) goto out_free_dev_nv;
	blk_queue_make_request(rdsk->rdsk_queue, rdsk_make_request);
	blk_queue_logical_block_size (rdsk->rdsk_queue, BYTES_PER_SECTOR);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	blk_queue_flush(rdsk->rdsk_queue, REQ_FLUSH);
#else
	blk_queue_ordered(rdsk->rdsk_queue, QUEUE_ORDERED_TAG, NULL);
#endif

	rdsk->rdsk_queue->limits.max_sectors = (max_sectors * 2);
	rdsk->rdsk_queue->nr_requests = nr_requests;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	rdsk->rdsk_queue->limits.discard_granularity = PAGE_SIZE;
	rdsk->rdsk_queue->limits.discard_zeroes_data = 1;
	rdsk->rdsk_queue->limits.max_discard_sectors = UINT_MAX;
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, rdsk->rdsk_queue);
#endif

	if (!(disk = rdsk->rdsk_disk = alloc_disk (1))) goto out_free_queue_nv;
	disk->major = rdsk_ma_no;
	disk->first_minor = num;
	disk->fops = &rdsk_fops;
	disk->private_data = rdsk;
	disk->queue = rdsk->rdsk_queue;
	disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "%s%d", RDPREFIX, num);
	set_capacity(disk, size);

	add_disk(rdsk->rdsk_disk);
	list_add_tail(&rdsk->rdsk_list, &rdsk_devices);
	rxcnt++;
	pr_info("%s: Attached rxd%d of %llu bytes in size.\n", RDPREFIX, num,
		(unsigned long long)(size * BYTES_PER_SECTOR));

	return 0;

out_free_queue_nv:
	blk_cleanup_queue(rdsk->rdsk_queue);

out_free_dev_nv:
	release_mem_region(rdsk->start_addr, rdsk->size);
	kfree(rdsk);
out_nv:
	return GENERIC_ERROR;
}

static int detach_device(int num)
{
	struct rdsk_device *rdsk;

	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list)
		if(rdsk->num == num) break;

	list_del(&rdsk->rdsk_list);
	del_gendisk(rdsk->rdsk_disk);
	put_disk(rdsk->rdsk_disk);
	blk_cleanup_queue(rdsk->rdsk_queue);
	if (rdsk->volatile_memory == VOLATILE_MEMORY) rdsk_free_pages(rdsk);
	else release_mem_region(rdsk->start_addr, rdsk->size);
	kfree(rdsk);
	rxcnt--;
	pr_info("%s: Detached rxd%d.\n", RDPREFIX, num);

	return 0;
}

static int resize_device(int num, int size)
{
	struct rdsk_device *rdsk;

	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list)
		if(rdsk->num == num) break;

	if (rdsk->volatile_memory == NON_VOLATILE_MEMORY){
		pr_warn("%s: Resizing is currently unsupported for non-volatile mappings.\n", RDPREFIX);
		return GENERIC_ERROR;
	}
	if (size <= get_capacity(rdsk->rdsk_disk)){
		pr_warn("%s: Please specify a larger size for resizing.\n", RDPREFIX);
		return GENERIC_ERROR;
	}
	set_capacity(rdsk->rdsk_disk, size);
	rdsk->size = (size * BYTES_PER_SECTOR);
	pr_info("%s: Resized rxd%d of %lu bytes in size.\n", RDPREFIX, num,
		(unsigned long)(size * BYTES_PER_SECTOR));
	return 0;
}

static int __init init_rxd(void)
{
	int retval;
	rdsk_ma_no = register_blkdev (rdsk_ma_no, RDPREFIX);
	if (rdsk_ma_no < 0){
		pr_err("%s: Failed registering rdsk, returned %d\n", RDPREFIX, rdsk_ma_no);
		return rdsk_ma_no;
	}

	rdsk_kobj = kobject_create_and_add("rapiddisk", kernel_kobj);
	if (!rdsk_kobj)
		goto init_failure;
	retval = sysfs_create_group(rdsk_kobj, &attr_group);
	if (retval) {
		kobject_put(rdsk_kobj);
		goto init_failure;
	}
	return 0;

init_failure:
	unregister_blkdev (rdsk_ma_no, RDPREFIX);
	return -ENOMEM;
}

static void __exit exit_rxd(void)
{
	struct rdsk_device *rdsk, *next;
	kobject_put(rdsk_kobj);
	list_for_each_entry_safe(rdsk, next, &rdsk_devices, rdsk_list)
		detach_device(rdsk->num);
	unregister_blkdev (rdsk_ma_no, RDPREFIX);
}

module_init(init_rxd);
module_exit(exit_rxd);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Petros Koutoupis <petros@petroskoutoupis.com>");
MODULE_DESCRIPTION("RapidDisk (rxdsk) is an enhanced RAM disk block device driver.");
MODULE_VERSION(VERSION_STR);
MODULE_INFO(Copyright, "Copyright 2010 - 2015 Petros Koutoupis");
