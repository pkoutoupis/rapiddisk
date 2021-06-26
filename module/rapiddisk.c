/*******************************************************************************
 ** Copyright © 2011 - 2021 Petros Koutoupis
 ** All rights reserved.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; under version 2 of the License.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 ** SPDX-License-Identifier: GPL-2.0-only
 **
 ** filename: rapiddisk.c
 ** description: RapidDisk is an enhanced Linux RAM disk module to dynamically
 **	 create, remove, and resize non-persistent RAM drives.
 ** created: 1Jun11, petros@petroskoutoupis.com
 **
 ******************************************************************************/

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
#include <linux/io.h>

#define VERSION_STR		"7.2.1"
#define PREFIX			"rapiddisk"
#define BYTES_PER_SECTOR	512
#define MAX_RDSKS		128
#define DEFAULT_MAX_SECTS	127
#define DEFAULT_REQUESTS	128
#define GENERIC_ERROR		-1

#define FREE_BATCH		16
#define SECTOR_SHIFT		9
#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		BIT(PAGE_SECTORS_SHIFT)

/* ioctls */
#define INVALID_CDQUERY_IOCTL	0x5331
#define RD_GET_STATS		0x0529

static DEFINE_MUTEX(sysfs_mutex);
static DEFINE_MUTEX(ioctl_mutex);

struct rdsk_device {
	int num;
	struct request_queue *rdsk_queue;
	struct gendisk *rdsk_disk;
	struct list_head rdsk_list;
	unsigned long long max_blk_alloc;	/* rdsk: to keep track of highest sector write	*/
	unsigned long long size;
	unsigned long error_cnt;
	spinlock_t rdsk_lock;
	struct radix_tree_root rdsk_pages;
};

static int rd_ma_no, rd_total; /* no. of attached devices */
static int rd_max_nr = MAX_RDSKS;
static int max_sectors = DEFAULT_MAX_SECTS, nr_requests = DEFAULT_REQUESTS;
static LIST_HEAD(rdsk_devices);
static struct kobject *rdsk_kobj;
static int rd_nr = 0;
int rd_size = 0;

module_param(max_sectors, int, S_IRUGO);
MODULE_PARM_DESC(max_sectors, " Maximum sectors (in KB) for the request queue. (Default = 127)");
module_param(nr_requests, int, S_IRUGO);
MODULE_PARM_DESC(nr_requests, " Number of requests at a given time for the request queue. (Default = 128)");
module_param(rd_nr, int, S_IRUGO);
MODULE_PARM_DESC(rd_nr, " Maximum number of RapidDisk devices to load on insertion. (Default = 0)");
module_param(rd_size, int, S_IRUGO);
MODULE_PARM_DESC(rd_size, " Size of each RAM disk (in KB) loaded on insertion. (Default = 0)");
module_param(rd_max_nr, int, S_IRUGO);
MODULE_PARM_DESC(rd_max_nr, " Maximum number of RAM Disks. (Default = 128)");

static int rdsk_do_bvec(struct rdsk_device *, struct page *,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
			unsigned int, unsigned int, bool, sector_t);
#else
			unsigned int, unsigned int, int, sector_t);
#endif
static int rdsk_ioctl(struct block_device *, fmode_t,
		      unsigned int, unsigned long);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
static blk_qc_t rdsk_submit_bio(struct bio *);
#else
static blk_qc_t rdsk_make_request(struct request_queue *, struct bio *);
#endif
#else
static void rdsk_make_request(struct request_queue *, struct bio *);
#endif
#else
static int rdsk_make_request(struct request_queue *, struct bio *);
#endif
static int attach_device(int);    /* disk size */
static int detach_device(int);	  /* disk num */
static int resize_device(int, int); /* disk num, disk size */
static ssize_t mgmt_show(struct kobject *, struct kobj_attribute *, char *);
static ssize_t mgmt_store(struct kobject *, struct kobj_attribute *,
			  const char *, size_t);

static ssize_t mgmt_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int len;
	struct rdsk_device *rdsk;

	len = sprintf(buf, "RapidDisk %s\n\nMaximum Number of Attachable Devices: %d\nNumber of Attached Devices: %d\nMax Sectors (KB): %d\nNumber of Requests: %d\n\n",
		      VERSION_STR, rd_max_nr, rd_total, max_sectors, nr_requests);
	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list) {
		len += sprintf(buf + len, "rd%d\tSize: %llu MBs\tErrors: %lu\n",
			       rdsk->num, (rdsk->size / 1024 / 1024),
			       rdsk->error_cnt);
	}
	return len;
}

static ssize_t mgmt_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buffer, size_t count)
{
	int num, size, err = (int)count;
	char *ptr, *buf;

	if (!buffer || count > PAGE_SIZE)
		return -EINVAL;

	mutex_lock(&sysfs_mutex);
	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		err = -ENOMEM;
		goto write_sysfs_error;
	}
	strcpy(buf, buffer);

	if (!strncmp("rapiddisk attach ", buffer, 17)) {
		ptr = buf + 17;
		size = (simple_strtoul(ptr, &ptr, 0) * 2);

		if (attach_device(size) != 0) {
			pr_err("%s: Unable to attach a new RapidDisk device.\n", PREFIX);
			err = -EINVAL;
		}
	} else if (!strncmp("rapiddisk detach ", buffer, 17)) {
		ptr = buf + 17;
		num = simple_strtoul(ptr, &ptr, 0);

		if (detach_device(num) != 0) {
			pr_err("%s: Unable to detach rd%d\n", PREFIX, num);
			err = -EINVAL;
		}
	} else if (!strncmp("rapiddisk resize ", buffer, 17)) {
		ptr = buf + 17;
		num = simple_strtoul(ptr, &ptr, 0);
		size = (simple_strtoul(ptr + 1, &ptr, 0) * 2);

		if (resize_device(num, size) != 0) {
			pr_err("%s: Unable to resize rd%d\n", PREFIX, num);
			err = -EINVAL;
		}
	} else {
		pr_err("%s: Unsupported command: %s\n", PREFIX, buffer);
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
	if (!page)
		return NULL;

	if (radix_tree_preload(GFP_NOIO)) {
		__free_page(page);
		return NULL;
	}

	spin_lock(&rdsk->rdsk_lock);
	idx = sector >> PAGE_SECTORS_SHIFT;
	if (radix_tree_insert(&rdsk->rdsk_pages, idx, page)) {
		__free_page(page);
		page = radix_tree_lookup(&rdsk->rdsk_pages, idx);
		BUG_ON(!page);
		BUG_ON(page->index != idx);
	} else {
		page->index = idx;
	}
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

	do {
		int i;

		nr_pages = radix_tree_gang_lookup(&rdsk->rdsk_pages,
						  (void **)pages, pos,
						  FREE_BATCH);

		for (i = 0; i < nr_pages; i++) {
			void *ret;

			BUG_ON(pages[i]->index < pos);
			pos = pages[i]->index;
			ret = radix_tree_delete(&rdsk->rdsk_pages, pos);
			BUG_ON(!ret || ret != pages[i]);
			__free_page(pages[i]);
		}
		pos++;
	} while (nr_pages == FREE_BATCH);
}

static int copy_to_rdsk_setup(struct rdsk_device *rdsk,
			      sector_t sector, size_t n)
{
	unsigned int offset = (sector & (PAGE_SECTORS - 1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	if (!rdsk_insert_page(rdsk, sector))
		return -ENOSPC;
	if (copy < n) {
		sector += copy >> SECTOR_SHIFT;
		if (!rdsk_insert_page(rdsk, sector))
			return -ENOSPC;
	}
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static void discard_from_rdsk(struct rdsk_device *rdsk,
			      sector_t sector, size_t n)
{
	while (n >= PAGE_SIZE) {
		rdsk_zero_page(rdsk, sector);
		sector += PAGE_SIZE >> SECTOR_SHIFT;
		n -= PAGE_SIZE;
	}
}
#endif

static void copy_to_rdsk(struct rdsk_device *rdsk, const void *src,
			 sector_t sector, size_t n)
{
	struct page *page;
	void *dst;
	unsigned int offset = (sector & (PAGE_SECTORS - 1)) << SECTOR_SHIFT;
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

	if (copy < n) {
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

	if ((sector + (n / BYTES_PER_SECTOR)) > rdsk->max_blk_alloc)
		rdsk->max_blk_alloc = (sector + (n / BYTES_PER_SECTOR));
}

static void copy_from_rdsk(void *dst, struct rdsk_device *rdsk,
			   sector_t sector, size_t n)
{
	struct page *page;
	void *src;
	unsigned int offset = (sector & (PAGE_SECTORS - 1)) << SECTOR_SHIFT;
	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = rdsk_lookup_page(rdsk, sector);

	if (page) {
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
	} else {
		memset(dst, 0, copy);
	}

	if (copy < n) {
		dst += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;
		page = rdsk_lookup_page(rdsk, sector);
		if (page) {
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
		} else {
			memset(dst, 0, copy);
		}
	}
}

static int rdsk_do_bvec(struct rdsk_device *rdsk, struct page *page,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
			unsigned int len, unsigned int off, bool is_write,
#else
			unsigned int len, unsigned int off, int rw,
#endif
			sector_t sector){
	void *mem;
	int err = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
	if (is_write) {
#else
	if (rw != READ) {
#endif
		err = copy_to_rdsk_setup(rdsk, sector, len);
		if (err)
			goto out;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	mem = kmap_atomic(page);
#else
	mem = kmap_atomic(page, KM_USER0);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
	if (!is_write) {
#else
	if (rw == READ) {
#endif
		copy_from_rdsk(mem + off, rdsk, sector, len);
		flush_dcache_page(page);
	} else {
		flush_dcache_page(page);
		copy_to_rdsk(rdsk, mem + off, sector, len);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	kunmap_atomic(mem);
#else
	kunmap_atomic(mem, KM_USER0);
#endif
out:
	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
static blk_qc_t
#else
static void
#endif
#else
static int
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
rdsk_submit_bio(struct bio *bio)
#else
rdsk_make_request(struct request_queue *q, struct bio *bio)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(5,12,0))
	struct rdsk_device *rdsk = bio->bi_disk->private_data;
#else
	struct block_device *bdev = bio->bi_bdev;
	struct rdsk_device *rdsk = bdev->bd_disk->private_data;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
	int rw;
#endif
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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(5,12,0))
	if ((sector + bio_sectors(bio)) > get_capacity(bio->bi_disk))
#else
	if ((sector + bio_sectors(bio)) > get_capacity(bdev->bd_disk))
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
		goto out;
#else
		goto io_error;
#endif

	err = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,336)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
	if (unlikely(bio_op(bio) == REQ_OP_DISCARD)) {
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
	if ((unlikely(bio_op(bio) == REQ_OP_DISCARD)) || (unlikely(bio_op(bio) == REQ_OP_WRITE_ZEROES))) {
#else
	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
#endif
		if (sector & ((PAGE_SIZE >> SECTOR_SHIFT) - 1) ||
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
		    bio->bi_iter.bi_size & ~PAGE_MASK)
#else
		    bio->bi_size & ~PAGE_MASK)
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
		goto io_error;
#else
		goto out;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
		discard_from_rdsk(rdsk, sector, bio->bi_iter.bi_size);
#else
		discard_from_rdsk(rdsk, sector, bio->bi_size);
#endif
		goto out;
	}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
	rw = bio_rw(bio);
	if (rw == READA)
		rw = READ;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;

		err = rdsk_do_bvec(rdsk, bvec.bv_page, len,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
				   bvec.bv_offset, op_is_write(bio_op(bio)), sector);
#else
				   bvec.bv_offset, rw, sector);
#endif
#else
	bio_for_each_segment(bvec, bio, i) {
		unsigned int len = bvec->bv_len;

		err = rdsk_do_bvec(rdsk, bvec->bv_page, len,
				   bvec->bv_offset, rw, sector);
#endif
		if (err) {
			rdsk->error_cnt++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
			break;
#else
			goto io_error;
#endif
		}
		sector += len >> SECTOR_SHIFT;
	}

out:
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
	set_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_endio(bio, err);
#else
	bio_endio(bio);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	return BLK_QC_T_NONE;
#else
	return;
#endif
io_error:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
	bio->bi_status= err;
#else
	bio->bi_error = err;
#endif
	bio_io_error(bio);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	return BLK_QC_T_NONE;
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	return 0;
#endif
}

static int rdsk_ioctl(struct block_device *bdev, fmode_t mode,
		      unsigned int cmd, unsigned long arg)
{
	loff_t size;
	int error = 0;
	struct rdsk_device *rdsk = bdev->bd_disk->private_data;

	switch (cmd) {
	case BLKGETSIZE:
		size = bdev->bd_inode->i_size;
		if ((size >> 9) > ~0UL)
			return -EFBIG;
		return copy_to_user((void __user *)arg, &size,
				    sizeof(size)) ? -EFAULT : 0;
	case BLKGETSIZE64:
		return copy_to_user((void __user *)arg,
				    &bdev->bd_inode->i_size,
				    sizeof(bdev->bd_inode->i_size)) ? -EFAULT : 0;
	case BLKFLSBUF:
		/* We are killing the RAM disk data. */
		mutex_lock(&ioctl_mutex);
		mutex_lock(&bdev->bd_mutex);
		error = -EBUSY;
		if (bdev->bd_openers <= 1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0) || (defined(RHEL_MAJOR) && RHEL_MAJOR == 8 && RHEL_MINOR >= 4)
			invalidate_bdev(bdev);
#else
			kill_bdev(bdev);
#endif
#else
			invalidate_bh_lrus();
			truncate_inode_pages(bdev->bd_inode->i_mapping, 0);
#endif
			rdsk_free_pages(rdsk);
			error = 0;
		}
		mutex_unlock(&bdev->bd_mutex);
		mutex_unlock(&ioctl_mutex);
		return error;
	case INVALID_CDQUERY_IOCTL:
		return -EINVAL;
	case RD_GET_STATS:
		return copy_to_user((void __user *)arg,
			&rdsk->max_blk_alloc,
			sizeof(rdsk->max_blk_alloc)) ? -EFAULT : 0;
	case BLKPBSZGET:
	case BLKBSZGET:
	case BLKSSZGET:
		size = BYTES_PER_SECTOR;
		return copy_to_user((void __user *)arg, &size,
			sizeof(size)) ? -EFAULT : 0;
	}

	pr_warn("%s: 0x%x invalid ioctl.\n", PREFIX, cmd);
	return -ENOTTY;		/* unknown command */
}

static const struct block_device_operations rdsk_fops = {
	.owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
	.submit_bio = rdsk_submit_bio,
#endif
	.ioctl = rdsk_ioctl,
};

static int attach_device(int size)
{
	int num = 0;
	struct rdsk_device *rdsk, *tmp;
	struct gendisk *disk;
	unsigned char *string, name[16];

	if (rd_total >= rd_max_nr) {
		pr_warn("%s: Reached maximum number of attached disks.\n",
			PREFIX);
		goto out;
	}

	string = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!string)
		goto out;
	list_for_each_entry(tmp, &rdsk_devices, rdsk_list) {
		sprintf(string, "%srd%d,", string, tmp->num);
		num++;
	}
	while (num >= 0) {
		memset(name, 0x0, sizeof(name));
		sprintf(name, "rd%d,", num);
                if (strstr(string, (const char *)name) == NULL) {
                        break;
                }
                num--;
        }
	kfree(string);

	rdsk = kzalloc(sizeof(*rdsk), GFP_KERNEL);
	if (!rdsk)
		goto out;
	rdsk->num = num;
	rdsk->error_cnt = 0;
	rdsk->max_blk_alloc = 0;
	rdsk->size = ((unsigned long long)size * BYTES_PER_SECTOR);
	spin_lock_init(&rdsk->rdsk_lock);
	INIT_RADIX_TREE(&rdsk->rdsk_pages, GFP_ATOMIC);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,9,0)
	rdsk->rdsk_queue = blk_alloc_queue(NUMA_NO_NODE);
#else
	rdsk->rdsk_queue = blk_alloc_queue(rdsk_make_request, NUMA_NO_NODE);
#endif
	if (!rdsk->rdsk_queue)
		goto out_free_dev;
#else
	rdsk->rdsk_queue = blk_alloc_queue(GFP_KERNEL);
	if (!rdsk->rdsk_queue)
		goto out_free_dev;
	blk_queue_make_request(rdsk->rdsk_queue, rdsk_make_request);
#endif
	blk_queue_logical_block_size(rdsk->rdsk_queue, BYTES_PER_SECTOR);
	blk_queue_physical_block_size(rdsk->rdsk_queue, PAGE_SIZE);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
	blk_queue_write_cache(rdsk->rdsk_queue, true, false);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	blk_queue_flush(rdsk->rdsk_queue, REQ_FLUSH);
#else
	blk_queue_ordered(rdsk->rdsk_queue, QUEUE_ORDERED_TAG, NULL);
#endif

	rdsk->rdsk_queue->limits.max_sectors = (max_sectors * 2);
	rdsk->rdsk_queue->nr_requests = nr_requests;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	rdsk->rdsk_queue->limits.discard_granularity = PAGE_SIZE;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0)
	rdsk->rdsk_queue->limits.discard_zeroes_data = 1;
#endif
	rdsk->rdsk_queue->limits.max_discard_sectors = UINT_MAX;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0)
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, rdsk->rdsk_queue);
#else
	blk_queue_flag_set(QUEUE_FLAG_DISCARD, rdsk->rdsk_queue);
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0)
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, rdsk->rdsk_queue);
#else
	blk_queue_flag_set(QUEUE_FLAG_NONROT, rdsk->rdsk_queue);
#endif

	disk = rdsk->rdsk_disk = alloc_disk(1);
	if (!disk)
		goto out_free_queue;
	disk->major = rd_ma_no;
	disk->first_minor = num;
	disk->fops = &rdsk_fops;
	disk->private_data = rdsk;
	disk->queue = rdsk->rdsk_queue;
	disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
	sprintf(disk->disk_name, "rd%d", num);
	set_capacity(disk, size);

	add_disk(rdsk->rdsk_disk);
	list_add_tail(&rdsk->rdsk_list, &rdsk_devices);
	rd_total++;
	pr_info("%s: Attached rd%d of %llu bytes in size.\n", PREFIX,
		num, rdsk->size);
	return 0;

out_free_queue:
	blk_cleanup_queue(rdsk->rdsk_queue);
out_free_dev:
	kfree(rdsk);
out:
	return GENERIC_ERROR;
}

static int detach_device(int num)
{
	struct rdsk_device *rdsk;

	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list)
		if (rdsk->num == num)
			break;

	list_del(&rdsk->rdsk_list);
	del_gendisk(rdsk->rdsk_disk);
	put_disk(rdsk->rdsk_disk);
	blk_cleanup_queue(rdsk->rdsk_queue);
	rdsk_free_pages(rdsk);
	kfree(rdsk);
	rd_total--;
	pr_info("%s: Detached rd%d.\n", PREFIX, num);

	return 0;
}

static int resize_device(int num, int size)
{
	struct rdsk_device *rdsk;

	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list)
		if (rdsk->num == num)
			break;

	if (size <= get_capacity(rdsk->rdsk_disk)) {
		pr_warn("%s: Please specify a larger size for resizing.\n",
			PREFIX);
		return GENERIC_ERROR;
	}
	set_capacity(rdsk->rdsk_disk, size);
	rdsk->size = (size * BYTES_PER_SECTOR);
	pr_info("%s: Resized rd%d of %lu bytes in size.\n", PREFIX, num,
	        (unsigned long)(size * BYTES_PER_SECTOR));
	return 0;
}

static int __init init_rd(void)
{
	int retval, i;

	rd_total = rd_ma_no = 0;
	rd_ma_no = register_blkdev(rd_ma_no, PREFIX);
	if (rd_ma_no < 0) {
		pr_err("%s: Failed registering rdsk, returned %d\n",
		       PREFIX, rd_ma_no);
		return rd_ma_no;
	}

	rdsk_kobj = kobject_create_and_add("rapiddisk", kernel_kobj);
	if (!rdsk_kobj)
		goto init_failure;
	retval = sysfs_create_group(rdsk_kobj, &attr_group);
	if (retval)
		goto init_failure2;

	for (i = 0; i < rd_nr; i++) {
		retval = attach_device(rd_size * 2);
		if (retval) {
			pr_err("%s: Failed to load RapidDisk volume rd%d.\n",
			       PREFIX, i);
			goto init_failure2;
		}
	}
	return 0;

init_failure2:
	kobject_put(rdsk_kobj);
init_failure:
	unregister_blkdev(rd_ma_no, PREFIX);
	return -ENOMEM;
}

static void __exit exit_rd(void)
{
	struct rdsk_device *rdsk, *next;

	kobject_put(rdsk_kobj);
	list_for_each_entry_safe(rdsk, next, &rdsk_devices, rdsk_list)
		detach_device(rdsk->num);
	unregister_blkdev(rd_ma_no, PREFIX);
}

module_init(init_rd);
module_exit(exit_rd);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Petros Koutoupis <petros@petroskoutoupis.com>");
MODULE_DESCRIPTION("RapidDisk is an enhanced RAM disk block device driver.");
MODULE_VERSION(VERSION_STR);
MODULE_INFO(Copyright, "Copyright 2010 - 2021 Petros Koutoupis");
