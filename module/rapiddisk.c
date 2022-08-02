/*******************************************************************************
 ** Copyright © 2011 - 2022 Petros Koutoupis
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

#define VERSION_STR		"8.2.0"
#define PREFIX			"rapiddisk"
#define BYTES_PER_SECTOR	512
#define MAX_RDSKS		1024
#define DEFAULT_MAX_SECTS	127
#define DEFAULT_REQUESTS	128
#define GENERIC_ERROR		-1
#define SUCCESS			0

#define FREE_BATCH		16
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
#define SECTOR_SHIFT		9
#define PAGE_SECTORS_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define PAGE_SECTORS		BIT(PAGE_SECTORS_SHIFT)
#endif

/* ioctls */
#define INVALID_CDQUERY_IOCTL	0x5331
#define INVALID_CDQUERY_IOCTL2	0x5395
#define RD_GET_STATS		0x0529
#define RD_GET_USAGE		0x0530

static DEFINE_MUTEX(sysfs_mutex);
static DEFINE_MUTEX(ioctl_mutex);

struct rdsk_device {
	int num;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
	struct request_queue *rdsk_queue;
#endif
	struct gendisk *rdsk_disk;
	struct list_head rdsk_list;
	unsigned long long max_blk_alloc;	/* rdsk: to keep track of highest sector write	*/
	unsigned long long max_page_cnt;
	unsigned long long size;
	unsigned long error_cnt;
	spinlock_t rdsk_lock;
	struct radix_tree_root rdsk_pages;
};

static unsigned long rd_max_nr = MAX_RDSKS, rd_ma_no, rd_total; /* no. of attached devices */
static unsigned long rd_size = 0, rd_nr = 0;
static int max_sectors = DEFAULT_MAX_SECTS, nr_requests = DEFAULT_REQUESTS;
static LIST_HEAD(rdsk_devices);
static struct kobject *rdsk_kobj;

module_param(max_sectors, int, S_IRUGO);
MODULE_PARM_DESC(max_sectors, " Maximum sectors (in KB) for the request queue. (Default = 127)");
module_param(nr_requests, int, S_IRUGO);
MODULE_PARM_DESC(nr_requests, " Number of requests at a given time for the request queue. (Default = 128)");
module_param(rd_nr, ulong, S_IRUGO);
MODULE_PARM_DESC(rd_nr, " Maximum number of RapidDisk devices to load on insertion. (Default = 0)");
module_param(rd_size, ulong, S_IRUGO);
MODULE_PARM_DESC(rd_size, " Size of each RAM disk (in MB) loaded on insertion. (Default = 0)");
module_param(rd_max_nr, ulong, S_IRUGO);
MODULE_PARM_DESC(rd_max_nr, " Maximum number of RAM Disks. (Default = 1024)");

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)
static void rdsk_submit_bio(struct bio *);
#else
static blk_qc_t rdsk_submit_bio(struct bio *);
#endif
#else
static blk_qc_t rdsk_make_request(struct request_queue *, struct bio *);
#endif
#else
static void rdsk_make_request(struct request_queue *, struct bio *);
#endif
#else
static int rdsk_make_request(struct request_queue *, struct bio *);
#endif
static int attach_device(unsigned long, unsigned long long); /* disk size */
static int detach_device(unsigned long);                     /* disk num */
static int resize_device(unsigned long, unsigned long long); /* disk num, disk size */
static ssize_t mgmt_show(struct kobject *, struct kobj_attribute *, char *);
static ssize_t mgmt_store(struct kobject *, struct kobj_attribute *,
			  const char *, size_t);
static ssize_t devices_show(struct kobject *, struct kobj_attribute *, char *);

static ssize_t mgmt_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf, "RapidDisk %s\n\nMaximum Number of Attachable Devices: %lu\n",
		      VERSION_STR, rd_max_nr);
	len += sprintf(buf + len, "Number of Attached Devices: %lu\nMax Sectors (KB): %d\nNumber of Requests: %d\n",
		       rd_total, max_sectors, nr_requests);

	return len;
}

static ssize_t devices_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int len = 0;
        struct rdsk_device *rdsk;

        mutex_lock(&sysfs_mutex);

        len += sprintf(buf + len, "Device\tSize\tErrors\tUsed\n");
        list_for_each_entry(rdsk, &rdsk_devices, rdsk_list) {
                len += scnprintf(buf + len, PAGE_SIZE - len, "rd%d\t%llu\t%lu\t%llu\n", rdsk->num,
                                 rdsk->size, rdsk->error_cnt, (rdsk->max_page_cnt * PAGE_SIZE));
        }

        mutex_unlock(&sysfs_mutex);
        return len;
}

static ssize_t mgmt_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buffer, size_t count)
{
	int err = (int)count;
	unsigned long num;
	unsigned long long size = 0;
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
		num = simple_strtoul(ptr, &ptr, 0);
		size = (simple_strtoull(ptr + 1, &ptr, 0));

		if (attach_device(num, size) != SUCCESS) {
			pr_err("%s: Unable to attach a new RapidDisk device.\n", PREFIX);
			err = -EINVAL;
		}
	} else if (!strncmp("rapiddisk detach ", buffer, 17)) {
		ptr = buf + 17;
		num = simple_strtoul(ptr, &ptr, 0);

		if (detach_device(num) != SUCCESS) {
			pr_err("%s: Unable to detach rd%lu\n", PREFIX, num);
			err = -EINVAL;
		}
	} else if (!strncmp("rapiddisk resize ", buffer, 17)) {
		ptr = buf + 17;
		num = simple_strtoul(ptr, &ptr, 0);
		size = (simple_strtoull(ptr + 1, &ptr, 0));

		if (resize_device(num, size) != SUCCESS) {
			pr_err("%s: Unable to resize rd%lu\n", PREFIX, num);
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

static struct kobj_attribute dev_attribute =
	__ATTR(devices, 0664, devices_show, NULL);

static struct attribute *attrs[] = {
	&mgmt_attribute.attr,
	&dev_attribute.attr,
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
	rdsk->max_page_cnt++;

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
	return SUCCESS;
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
	int err = SUCCESS;

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)
static void
#else
static blk_qc_t
#endif
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

	err = SUCCESS;
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)
				   bvec.bv_offset, bio_op(bio), sector);
#else
				   bvec.bv_offset, op_is_write(bio_op(bio)), sector);
#endif
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
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,16,0)
	return BLK_QC_T_NONE;
#else
	return;
#endif
#endif
io_error:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
	bio->bi_status= err;
#else
	bio->bi_error = err;
#endif
	bio_io_error(bio);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,16,0)
	return BLK_QC_T_NONE;
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
	return SUCCESS;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)
static inline int bdev_openers(struct block_device *bdev)
{
	return atomic_read(&bdev->bd_openers);
}
#else
static inline int bdev_openers(struct block_device *bdev)
{
	return bdev->bd_openers;
}
#endif

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
		mutex_lock(&bdev->bd_disk->open_mutex);
#else
		mutex_lock(&bdev->bd_mutex);
#endif
		error = -EBUSY;
		if (bdev_openers(bdev) <= 1) {
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
		mutex_unlock(&bdev->bd_disk->open_mutex);
#else
		mutex_unlock(&bdev->bd_mutex);
#endif
		rdsk->max_blk_alloc = 0;
		rdsk->max_page_cnt = 0;
		mutex_unlock(&ioctl_mutex);
		return error;
	case INVALID_CDQUERY_IOCTL:
	case INVALID_CDQUERY_IOCTL2:
		return -EINVAL;
	case RD_GET_STATS:
		return copy_to_user((void __user *)arg,
			&rdsk->max_blk_alloc,
			sizeof(rdsk->max_blk_alloc)) ? -EFAULT : 0;
	case RD_GET_USAGE:
		return copy_to_user((void __user *)arg,
			&rdsk->max_page_cnt,
			sizeof(rdsk->max_page_cnt)) ? -EFAULT : 0;
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

static int attach_device(unsigned long num, unsigned long long size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)
	int err = GENERIC_ERROR;
#endif
	struct rdsk_device *rdsk, *tmp;
	struct gendisk *disk;
	sector_t sectors = 0;

	if (num > MINORMASK) {
		pr_warn("%s: Reached maximum value for the attached disk.\n", PREFIX);
		goto out;
	}

	if (rd_total >= rd_max_nr) {
		pr_warn("%s: Reached maximum number of attached disks.\n", PREFIX);
		goto out;
	}

	if (size % BYTES_PER_SECTOR != 0) {
		pr_err("%s: Invalid size input. Size must be a multiple of sector size %d.\n",
		       PREFIX, BYTES_PER_SECTOR);
		goto out;
	}
	sectors = (size / BYTES_PER_SECTOR);


	list_for_each_entry(tmp, &rdsk_devices, rdsk_list) {
		if (tmp->num == num) goto out;
	}

	rdsk = kzalloc(sizeof(*rdsk), GFP_KERNEL);
	if (!rdsk)
		goto out;
	rdsk->num = num;
	rdsk->error_cnt = 0;
	rdsk->max_blk_alloc = 0;
	rdsk->max_page_cnt = 0;
	rdsk->size = size;
	spin_lock_init(&rdsk->rdsk_lock);
	INIT_RADIX_TREE(&rdsk->rdsk_pages, GFP_ATOMIC);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0)
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
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
	disk = rdsk->rdsk_disk = blk_alloc_disk(NUMA_NO_NODE);
#else
	disk = rdsk->rdsk_disk = alloc_disk(1);
#endif
	if (!disk)
		goto out_free_queue;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
	disk = rdsk->rdsk_disk = blk_alloc_disk(NUMA_NO_NODE);
#else
	disk = rdsk->rdsk_disk = alloc_disk(1);
#endif
	if (!disk)
		goto out_free_queue;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
	blk_queue_logical_block_size(disk->queue, BYTES_PER_SECTOR);
	blk_queue_physical_block_size(disk->queue, PAGE_SIZE);
#else
	blk_queue_logical_block_size(rdsk->rdsk_queue, BYTES_PER_SECTOR);
	blk_queue_physical_block_size(rdsk->rdsk_queue, PAGE_SIZE);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
	blk_queue_write_cache(disk->queue, true, false);
#else
	blk_queue_write_cache(rdsk->rdsk_queue, true, false);
#endif
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	blk_queue_flush(rdsk->rdsk_queue, REQ_FLUSH);
#else
	blk_queue_ordered(rdsk->rdsk_queue, QUEUE_ORDERED_TAG, NULL);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
	disk->queue->limits.max_sectors = (max_sectors * 2);
	disk->queue->nr_requests = nr_requests;
	disk->queue->limits.discard_granularity = PAGE_SIZE;
	disk->queue->limits.max_discard_sectors = UINT_MAX;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,19,0)
	blk_queue_max_discard_sectors(disk->queue, 0);
#else
	blk_queue_flag_set(QUEUE_FLAG_DISCARD, disk->queue);
#endif
	blk_queue_flag_set(QUEUE_FLAG_NONROT, disk->queue);
#else
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
#endif

	disk->major = rd_ma_no;
	disk->first_minor = num;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
	disk->minors = 1;
#endif
	disk->fops = &rdsk_fops;
	disk->private_data = rdsk;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0)
	disk->queue = rdsk->rdsk_queue;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
	disk->flags |= GENHD_FL_NO_PART;
#else
	disk->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
#endif
	sprintf(disk->disk_name, "rd%lu", num);
	set_capacity(disk, sectors);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)
	err = add_disk(disk);
	if (err)
		goto out_free_queue;
#else
	add_disk(disk);
#endif
	list_add_tail(&rdsk->rdsk_list, &rdsk_devices);
	rd_total++;
	pr_info("%s: Attached rd%lu of %llu bytes in size.\n", PREFIX, num, rdsk->size);
	return SUCCESS;

out_free_queue:
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0)
	blk_cleanup_queue(rdsk->rdsk_queue);
out_free_dev:
#endif
	kfree(rdsk);
out:
	return GENERIC_ERROR;
}

static int detach_device(unsigned long num)
{
	struct rdsk_device *rdsk;
	bool found = false;

	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list)
		if (rdsk->num == num) {
			found = true;
			break;
		}

	if (!found)
		return GENERIC_ERROR;

	list_del(&rdsk->rdsk_list);
	del_gendisk(rdsk->rdsk_disk);
	put_disk(rdsk->rdsk_disk);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
	blk_cleanup_queue(rdsk->rdsk_queue);
#endif
	rdsk_free_pages(rdsk);
	kfree(rdsk);
	rd_total--;
	pr_info("%s: Detached rd%lu.\n", PREFIX, num);

	return SUCCESS;
}

static int resize_device(unsigned long num, unsigned long long size)
{
	struct rdsk_device *rdsk;
	bool found = false;
	sector_t sectors = 0;

	if (size % BYTES_PER_SECTOR != 0) {
		pr_err("%s: Invalid size input. Size must be a multiple of sector size %d.\n",
		       PREFIX, BYTES_PER_SECTOR);
		return GENERIC_ERROR;
	}
	sectors = (size / BYTES_PER_SECTOR);

	list_for_each_entry(rdsk, &rdsk_devices, rdsk_list)
		if (rdsk->num == num) {
			found = true;
			break;
		}

	if (!found)
		return GENERIC_ERROR;

	if (size <= get_capacity(rdsk->rdsk_disk)) {
		pr_warn("%s: Please specify a larger size for resizing.\n",
			PREFIX);
		return GENERIC_ERROR;
	}
	set_capacity(rdsk->rdsk_disk, sectors);
	rdsk->size = size;
	pr_info("%s: Resized rd%lu of %llu bytes in size.\n", PREFIX, num, size);
	return SUCCESS;
}

static int __init init_rd(void)
{
	int retval, i;

	rd_total = rd_ma_no = 0;
	rd_ma_no = register_blkdev(rd_ma_no, PREFIX);
	if (rd_ma_no < 0) {
		pr_err("%s: Failed registering rdsk, returned %lu\n",
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
		retval = attach_device(i, rd_size * 2048);
		if (retval) {
			pr_err("%s: Failed to load RapidDisk volume rd%d.\n",
			       PREFIX, i);
			goto init_failure2;
		}
	}
	return SUCCESS;

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
MODULE_INFO(Copyright, "Copyright 2010 - 2022 Petros Koutoupis");
