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
 ** filename: rapiddisk-cache.c
 ** description: Device mapper target for block-level disk write-through and
 **	 write-around caching. This module is based on Flashcache-wt:
 **	  Copyright 2010 Facebook, Inc.
 **	  Author: Mohan Srinivasan (mohan@facebook.com)
 **
 **	 Which in turn was based on DM-Cache:
 **	  Copyright (C) International Business Machines Corp., 2006
 **	  Author: Ming Zhao (mingzhao@ufl.edu)
 **
 ** created: 3Dec11, petros@petroskoutoupis.com
 **
 ******************************************************************************/

#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <linux/hardirq.h>
#include <linux/dm-io.h>
#include <linux/device-mapper.h>

#define ASSERT(x) do { \
	if (unlikely(!(x))) { \
		dump_stack(); \
		panic("ASSERT: assertion (%s) failed at %s (%d)\n", \
		#x,  __FILE__, __LINE__); \
	} \
} while (0)

#define VERSION_STR	"8.2.0"
#define DM_MSG_PREFIX	"rapiddisk-cache"

#define READCACHE	1
#define WRITECACHE	2
#define READSOURCE	3
#define WRITESOURCE	4
#define READCACHE_DONE	5

#define WRITETHROUGH	0
#define WRITEAROUND	1

#define GENERIC_ERROR		-1
#define BYTES_PER_BLOCK		512
/* Default cache parameters */
#define DEFAULT_CACHE_ASSOC	512
#define CACHE_BLOCK_SIZE	(PAGE_SIZE / BYTES_PER_BLOCK)
#define CONSECUTIVE_BLOCKS	512

/* States of a cache block */
#define INVALID		0
#define VALID		1
#define INPROG		2	/* IO (cache fill) is in progress */
#define CACHEREADINPROG	3
#define INPROG_INVALID	4	/* Write invalidated during a refill */

#define DEV_PATHLEN	128

#ifndef DM_MAPIO_SUBMITTED
#define DM_MAPIO_SUBMITTED	0
#endif

#define WT_MIN_JOBS	1024
/* Number of pages for I/O */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,39)
#define COPY_PAGES (1024)
#endif

/* Cache context */
struct cache_context {
	struct dm_target *tgt;
	struct dm_dev *disk_dev;	/* Source device */
	struct dm_dev *cache_dev;	/* Cache device */

	spinlock_t cache_spin_lock;
	struct cache_block *cache;
	u8 *cache_state;
	u32 *set_lru_next;
	int mode;			/* Write Through / Around */

	struct dm_io_client *io_client;
	sector_t size;
	unsigned int assoc;
	unsigned int block_size;
	unsigned int block_shift;
	unsigned int block_mask;
	unsigned int consecutive_shift;

	wait_queue_head_t destroyq;	/* Wait queue for I/O completion */
	atomic_t nr_jobs;		/* Number of I/O jobs */

	/* Stats */
	unsigned long reads;
	unsigned long writes;
	unsigned long cache_hits;
	unsigned long replace;
	unsigned long wr_invalidates;
	unsigned long rd_invalidates;
	unsigned long cached_blocks;
	unsigned long cache_wr_replace;
	unsigned long uncached_reads;
	unsigned long uncached_writes;
	unsigned long cache_reads, cache_writes;
	unsigned long disk_reads, disk_writes;

	char cache_devname[DEV_PATHLEN];
	char disk_devname[DEV_PATHLEN];
};

/* Cache block metadata structure */
struct cache_block {
	sector_t dbn;		/* Sector number of the cached block */
};

/* Structure for a kcached job */
struct kcached_job {
	struct list_head list;
	struct cache_context *dmc;
	struct bio *bio;	/* Original bio */
	struct dm_io_region disk;
	struct dm_io_region cache;
	int index;
	int rw;
	int error;
};

static struct workqueue_struct *kcached_wq;
static struct work_struct kcached_work;
static struct kmem_cache *job_cache;
static mempool_t *job_pool;
static DEFINE_SPINLOCK(job_lock);
static LIST_HEAD(complete_jobs);
static LIST_HEAD(io_jobs);
static void cache_read_miss(struct cache_context *, struct bio *, int);
static void cache_write(struct cache_context *, struct bio *);
static int cache_invalidate_blocks(struct cache_context *, struct bio *);
static void rc_uncached_io_callback(unsigned long, void *);
static void rc_start_uncached_io(struct cache_context *, struct bio *);

int dm_io_async_bvec(unsigned int num_regions, struct dm_io_region *where,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
		     int rw, struct bio *bio, io_notify_fn fn, void *context)
#else
		     int rw, struct bio_vec *bvec, io_notify_fn fn, void *context)
#endif
{
	struct kcached_job *job = (struct kcached_job *)context;
	struct cache_context *dmc = job->dmc;
	struct dm_io_request iorq;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0)
	(rw == WRITE) ? (iorq.bi_opf = REQ_OP_WRITE) : (iorq.bi_opf = REQ_OP_READ);
#else
	iorq.bi_op = rw;
	iorq.bi_op_flags = 0;
#endif
#else
	iorq.bi_rw = rw;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	iorq.mem.type = DM_IO_BIO;
	iorq.mem.ptr.bio = bio;
#else
	iorq.mem.type = DM_IO_BVEC;
	iorq.mem.ptr.bvec = bvec;
#endif
	iorq.notify.fn = fn;
	iorq.notify.context = context;
	iorq.client = dmc->io_client;

	return dm_io(&iorq, num_regions, where, NULL);
}

static int jobs_init(void)
{
	job_cache = kmem_cache_create("kcached-jobs-wt",
				      sizeof(struct kcached_job),
				      __alignof__(struct kcached_job),
				      0, NULL);
	if (!job_cache)
		return -ENOMEM;

	job_pool = mempool_create(WT_MIN_JOBS, mempool_alloc_slab,
				  mempool_free_slab, job_cache);
	if (!job_pool) {
		kmem_cache_destroy(job_cache);
		return -ENOMEM;
	}
	return 0;
}

static void jobs_exit(void)
{
	BUG_ON(!list_empty(&complete_jobs));
	BUG_ON(!list_empty(&io_jobs));

	mempool_destroy(job_pool);
	kmem_cache_destroy(job_cache);
	job_pool = NULL;
	job_cache = NULL;
}

static inline struct kcached_job *pop(struct list_head *jobs)
{
	struct kcached_job *job = NULL;
	unsigned long flags;

	spin_lock_irqsave(&job_lock, flags);
	if (!list_empty(jobs)) {
		job = list_entry(jobs->next, struct kcached_job, list);
		list_del(&job->list);
	}
	spin_unlock_irqrestore(&job_lock, flags);
	return job;
}

static inline void push(struct list_head *jobs, struct kcached_job *job)
{
	unsigned long flags;

	spin_lock_irqsave(&job_lock, flags);
	list_add_tail(&job->list, jobs);
	spin_unlock_irqrestore(&job_lock, flags);
}

void rc_io_callback(unsigned long error, void *context)
{
	struct kcached_job *job = (struct kcached_job *)context;
	struct cache_context *dmc = job->dmc;
	struct bio *bio;
	unsigned long flags;
	int invalid = 0;

	ASSERT(job);
	bio = job->bio;
	ASSERT(bio);
	if (error)
		DMERR("%s: io error %ld", __func__, error);
	if (job->rw == READSOURCE || job->rw == WRITESOURCE) {
		spin_lock_irqsave(&dmc->cache_spin_lock, flags);
		if (dmc->cache_state[job->index] != INPROG) {
			ASSERT(dmc->cache_state[job->index] == INPROG_INVALID);
			invalid++;
		}
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		if (error || invalid) {
			if (invalid)
				DMERR("%s: cache fill invalidation, sector %lu, size %u",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
				      __func__,
				      (unsigned long)bio->bi_iter.bi_sector,
				      bio->bi_iter.bi_size);
#else
				      __func__, (unsigned long)bio->bi_sector,
				      bio->bi_size);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
			bio_endio(bio, error);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
			bio->bi_status= error;
#else
			bio->bi_error = error;
#endif
			bio_io_error(bio);
#endif
			spin_lock_irqsave(&dmc->cache_spin_lock, flags);
			dmc->cache_state[job->index] = INVALID;
			spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
			goto out;
		} else {
			job->rw = WRITECACHE;
			push(&io_jobs, job);
			queue_work(kcached_wq, &kcached_work);
			return;
		}
	} else if (job->rw == READCACHE) {
		spin_lock_irqsave(&dmc->cache_spin_lock, flags);
		ASSERT(dmc->cache_state[job->index] == INPROG_INVALID ||
		       dmc->cache_state[job->index] ==  CACHEREADINPROG);
		if (dmc->cache_state[job->index] == INPROG_INVALID)
			invalid++;
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		if (!invalid && !error) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
			bio_endio(bio, 0);
#else
			bio_endio(bio);
#endif
			spin_lock_irqsave(&dmc->cache_spin_lock, flags);
			dmc->cache_state[job->index] = VALID;
			spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
			goto out;
		}
		/* error || invalid || bounce back to source device */
		job->rw = READCACHE_DONE;
		push(&complete_jobs, job);
		queue_work(kcached_wq, &kcached_work);
		return;
	} else {
		ASSERT(job->rw == WRITECACHE);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
		bio_endio(bio, 0);
#else
		bio_endio(bio);
#endif
		spin_lock_irqsave(&dmc->cache_spin_lock, flags);
		ASSERT((dmc->cache_state[job->index] == INPROG) ||
		       (dmc->cache_state[job->index] == INPROG_INVALID));
		if (error || dmc->cache_state[job->index] == INPROG_INVALID) {
			dmc->cache_state[job->index] = INVALID;
		} else {
			dmc->cache_state[job->index] = VALID;
			dmc->cached_blocks++;
		}
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
	}
out:
	mempool_free(job, job_pool);
	if (atomic_dec_and_test(&dmc->nr_jobs))
		wake_up(&dmc->destroyq);
}
EXPORT_SYMBOL(rc_io_callback);

static int do_io(struct kcached_job *job)
{
	int r = 0;
	struct cache_context *dmc = job->dmc;
	struct bio *bio = job->bio;

	ASSERT(job->rw == WRITECACHE);
	dmc->cache_writes++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	r = dm_io_async_bvec(1, &job->cache, WRITE, bio, rc_io_callback, job);
#else
	r = dm_io_async_bvec(1, &job->cache, WRITE, bio->bi_io_vec + bio->bi_idx, rc_io_callback, job);
#endif
	ASSERT(r == 0); /* dm_io_async_bvec() must always return 0 */
	return r;
}

int rc_do_complete(struct kcached_job *job)
{
	struct bio *bio = job->bio;
	struct cache_context *dmc = job->dmc;
	unsigned long flags;

	ASSERT(job->rw == READCACHE_DONE);
	/* error || block invalidated while reading from cache */
	spin_lock_irqsave(&dmc->cache_spin_lock, flags);
	dmc->cache_state[job->index] = INVALID;
	spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
	mempool_free(job, job_pool);
	if (atomic_dec_and_test(&dmc->nr_jobs))
		wake_up(&dmc->destroyq);
	/* Kick this IO back to the source bdev */
	rc_start_uncached_io(dmc, bio);
	return 0;
}
EXPORT_SYMBOL(rc_do_complete);

static void process_jobs(struct list_head *jobs,
			 int (*fn)(struct kcached_job *))
{
	struct kcached_job *job;

	while ((job = pop(jobs)))
		(void)fn(job);
}

static void do_work(struct work_struct *work)
{
	process_jobs(&complete_jobs, rc_do_complete);
	process_jobs(&io_jobs, do_io);
}

static int kcached_init(struct cache_context *dmc)
{
	init_waitqueue_head(&dmc->destroyq);
	atomic_set(&dmc->nr_jobs, 0);
	return 0;
}

void kcached_client_destroy(struct cache_context *dmc)
{
	wait_event(dmc->destroyq, !atomic_read(&dmc->nr_jobs));
}

static unsigned long hash_block(struct cache_context *dmc, sector_t dbn)
{
	unsigned long set_number;
	uint64_t value;

	value = (dbn >> (dmc->block_shift + dmc->consecutive_shift));
	set_number = do_div(value, (dmc->size >> dmc->consecutive_shift));
	return set_number;
}

static int find_valid_dbn(struct cache_context *dmc, sector_t dbn,
			  int start_index, int *index)
{
	int i;
	int end_index = start_index + dmc->assoc;

	for (i = start_index ; i < end_index ; i++) {
		if (dbn == dmc->cache[i].dbn &&
		    (dmc->cache_state[i] == VALID ||
		    dmc->cache_state[i] == CACHEREADINPROG ||
		    dmc->cache_state[i] == INPROG)) {
			*index = i;
			return dmc->cache_state[i];
		}
	}
	return GENERIC_ERROR;
}

static void find_invalid_dbn(struct cache_context *dmc,
			     int start_index, int *index)
{
	int i;
	int end_index = start_index + dmc->assoc;

	/* Find INVALID slot that we can reuse */
	for (i = start_index ; i < end_index ; i++) {
		if (dmc->cache_state[i] == INVALID) {
			*index = i;
			return;
		}
	}
}

static void find_reclaim_dbn(struct cache_context *dmc,
			     int start_index, int *index)
{
	int i;
	int end_index = start_index + dmc->assoc;
	int set = start_index / dmc->assoc;
	int slots_searched = 0;

	/* Find the "oldest" VALID slot to recycle. For each set, we keep
	 * track of the next "lru" slot to pick off. Each time we pick off
	 * a VALID entry to recycle we advance this pointer. So  we sweep
	 * through the set looking for next blocks to recycle. This
	 * approximates to FIFO (modulo for blocks written through). */
	i = dmc->set_lru_next[set];
	while (slots_searched < dmc->assoc) {
		ASSERT(i >= start_index);
		ASSERT(i < end_index);
		if (dmc->cache_state[i] == VALID) {
			*index = i;
			break;
		}
		slots_searched++;
		i++;
		if (i == end_index)
			i = start_index;
	}
	i++;
	if (i == end_index)
		i = start_index;
	dmc->set_lru_next[set] = i;
}

/* dbn is the starting sector, io_size is the number of sectors. */
static int cache_lookup(struct cache_context *dmc, struct bio *bio, int *index)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	sector_t dbn = bio->bi_iter.bi_sector;
#else
	sector_t dbn = bio->bi_sector;
#endif
	unsigned long set_number = hash_block(dmc, dbn);
	int invalid = -1, oldest_clean = -1;
	int start_index;
	int ret;

	start_index = dmc->assoc * set_number;
	ret = find_valid_dbn(dmc, dbn, start_index, index);
	if (ret == VALID || ret == INPROG || ret == CACHEREADINPROG) {
		/* We found the exact range of blocks we are looking for */
		return ret;
	}
	ASSERT(ret == -1);
	find_invalid_dbn(dmc, start_index, &invalid);
	if (invalid == -1) {
		/* We didn't find an invalid entry,
		 * search for oldest valid entry */
		find_reclaim_dbn(dmc, start_index, &oldest_clean);
	}
	/* Cache miss : We can't choose an entry marked INPROG,
	 * but choose the oldest INVALID or the oldest VALID entry. */
	*index = start_index + dmc->assoc;
	if (invalid != -1)
		*index = invalid;
	else if (oldest_clean != -1)
		*index = oldest_clean;
	if (*index < (start_index + dmc->assoc))
		return INVALID;
	else
		return GENERIC_ERROR;
}

static struct kcached_job *new_kcached_job(struct cache_context *dmc,
					   struct bio *bio, int index)
{
	struct kcached_job *job;

	job = mempool_alloc(job_pool, GFP_NOIO);
	if (!job)
		return NULL;
	job->disk.bdev = dmc->disk_dev->bdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	job->disk.sector = bio->bi_iter.bi_sector;
#else
	job->disk.sector = bio->bi_sector;
#endif
	if (index != -1)
		job->disk.count = dmc->block_size;
	else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
		job->disk.count = to_sector(bio->bi_iter.bi_size);
#else
		job->disk.count = to_sector(bio->bi_size);
#endif
	job->cache.bdev = dmc->cache_dev->bdev;
	if (index != -1) {
		job->cache.sector = index << dmc->block_shift;
		job->cache.count = dmc->block_size;
	}
	job->dmc = dmc;
	job->bio = bio;
	job->index = index;
	job->error = 0;
	return job;
}

static void cache_read_miss(struct cache_context *dmc,
			    struct bio *bio, int index)
{
	struct kcached_job *job;
	unsigned long flags;

	job = new_kcached_job(dmc, bio, index);
	if (unlikely(!job)) {
		DMERR("%s: Cannot allocate job\n", __func__);
		spin_lock_irqsave(&dmc->cache_spin_lock, flags);
		dmc->cache_state[index] = INVALID;
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
		bio_endio(bio, -EIO);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
		bio->bi_status = -EIO;
#else
		bio->bi_error = -EIO;
#endif
		bio_io_error(bio);
#endif
	} else {
		job->rw = READSOURCE;
		atomic_inc(&dmc->nr_jobs);
		dmc->disk_reads++;
		dm_io_async_bvec(1, &job->disk, READ,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
				 bio, rc_io_callback, job);
#else
				 bio->bi_io_vec + bio->bi_idx,
				 rc_io_callback, job);
#endif
	}
}

static void cache_read(struct cache_context *dmc, struct bio *bio)
{
	int index;
	int res;
	unsigned long flags;

	spin_lock_irqsave(&dmc->cache_spin_lock, flags);
	res = cache_lookup(dmc, bio, &index);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	if ((res == VALID) &&
	    (dmc->cache[index].dbn == bio->bi_iter.bi_sector)) {
#else
	if ((res == VALID) &&
	    (dmc->cache[index].dbn == bio->bi_sector)) {
#endif
		struct kcached_job *job;

		dmc->cache_state[index] = CACHEREADINPROG;
		dmc->cache_hits++;
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		job = new_kcached_job(dmc, bio, index);
		if (unlikely(!job)) {
			DMERR("cache_read(_hit): Cannot allocate job\n");
			spin_lock_irqsave(&dmc->cache_spin_lock, flags);
			dmc->cache_state[index] = VALID;
			spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
			bio_endio(bio, -EIO);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
			bio->bi_status = -EIO;
#else
			bio->bi_error = -EIO;
#endif
			bio_io_error(bio);
#endif
		} else {
			job->rw = READCACHE;
			atomic_inc(&dmc->nr_jobs);
			dmc->cache_reads++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
			dm_io_async_bvec(1, &job->cache, READ, bio,
					 rc_io_callback, job);
#else
			dm_io_async_bvec(1, &job->cache, READ,
					 bio->bi_io_vec + bio->bi_idx,
					 rc_io_callback, job);
#endif
		}
		return;
	}
	if (cache_invalidate_blocks(dmc, bio) > 0) {
		/* A non zero return indicates an inprog invalidation */
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		rc_start_uncached_io(dmc, bio);
		return;
	}
	if (res == -1 || res >= INPROG) {
		/* We either didn't find a cache slot in the set we were
		 * looking at or the block we are trying to read is being
		 * refilled into cache. */
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		rc_start_uncached_io(dmc, bio);
		return;
	}
	/* (res == INVALID) Cache Miss And we found cache blocks to replace
	 * Claim the cache blocks before giving up the spinlock */
	if (dmc->cache_state[index] == VALID) {
		dmc->cached_blocks--;
		dmc->replace++;
	}
	dmc->cache_state[index] = INPROG;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	dmc->cache[index].dbn = bio->bi_iter.bi_sector;
#else
	dmc->cache[index].dbn = bio->bi_sector;
#endif
	spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
	cache_read_miss(dmc, bio, index);
}

static int cache_invalidate_block_set(struct cache_context *dmc, int set,
				      sector_t io_start, sector_t io_end,
				      int rw, int *inprog_inval)
{
	int start_index, end_index, i;
	int invalidations = 0;

	start_index = dmc->assoc * set;
	end_index = start_index + dmc->assoc;
	for (i = start_index ; i < end_index ; i++) {
		sector_t start_dbn = dmc->cache[i].dbn;
		sector_t end_dbn = start_dbn + dmc->block_size;

		if (dmc->cache_state[i] == INVALID ||
		    dmc->cache_state[i] == INPROG_INVALID)
			continue;
		if ((io_start >= start_dbn && io_start < end_dbn) ||
		    (io_end >= start_dbn && io_end < end_dbn)) {
			if (rw == WRITE)
				dmc->wr_invalidates++;
			else
				dmc->rd_invalidates++;
			invalidations++;
			if (dmc->cache_state[i] == VALID) {
				dmc->cached_blocks--;
				dmc->cache_state[i] = INVALID;
			} else if (dmc->cache_state[i] >= INPROG) {
				(*inprog_inval)++;
				dmc->cache_state[i] = INPROG_INVALID;
				DMERR("%s: sector %lu, size %lu, rw %d",
				      __func__, (unsigned long)io_start,
				      (unsigned long)io_end - (unsigned long)io_start, rw);
			}
		}
	}
	return invalidations;
}

static int cache_invalidate_blocks(struct cache_context *dmc, struct bio *bio)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	sector_t io_start = bio->bi_iter.bi_sector;
	sector_t io_end = bio->bi_iter.bi_sector + (to_sector(bio->bi_iter.bi_size) - 1);
#else
	sector_t io_start = bio->bi_sector;
	sector_t io_end = bio->bi_sector + (to_sector(bio->bi_size) - 1);
#endif
	int start_set, end_set;
	int inprog_inval_start = 0, inprog_inval_end = 0;

	start_set = hash_block(dmc, io_start);
	end_set = hash_block(dmc, io_end);
	cache_invalidate_block_set(dmc, start_set, io_start, io_end,
				   bio_data_dir(bio), &inprog_inval_start);
	if (start_set != end_set)
		cache_invalidate_block_set(dmc, end_set, io_start, io_end,
					   bio_data_dir(bio),
					   &inprog_inval_end);
	return (inprog_inval_start + inprog_inval_end);
}

static void cache_write(struct cache_context *dmc, struct bio *bio)
{
	int index;
	int res;
	unsigned long flags;
	struct kcached_job *job;

	spin_lock_irqsave(&dmc->cache_spin_lock, flags);
	if (cache_invalidate_blocks(dmc, bio) > 0) {
		/* A non zero return indicates an inprog invalidation */
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		rc_start_uncached_io(dmc, bio);
		return;
	}
	res = cache_lookup(dmc, bio, &index);
	ASSERT(res == -1 || res == INVALID);
	if (res == -1) {
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		rc_start_uncached_io(dmc, bio);
		return;
	}
	if (dmc->cache_state[index] == VALID) {
		dmc->cached_blocks--;
		dmc->cache_wr_replace++;
	}
	dmc->cache_state[index] = INPROG;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	dmc->cache[index].dbn = bio->bi_iter.bi_sector;
#else
	dmc->cache[index].dbn = bio->bi_sector;
#endif
	spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
	job = new_kcached_job(dmc, bio, index);
	if (unlikely(!job)) {
		DMERR("%s: Cannot allocate job\n", __func__);
		spin_lock_irqsave(&dmc->cache_spin_lock, flags);
		dmc->cache_state[index] = INVALID;
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
		bio_endio(bio, -EIO);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
		bio->bi_status= -EIO;
#else
		bio->bi_error = -EIO;
#endif
		bio_io_error(bio);
#endif
		return;
	}
	job->rw = WRITESOURCE;
	atomic_inc(&job->dmc->nr_jobs);
	dmc->disk_writes++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	dm_io_async_bvec(1, &job->disk, WRITE, bio, rc_io_callback, job);
#else
	dm_io_async_bvec(1, &job->disk, WRITE, bio->bi_io_vec + bio->bi_idx, rc_io_callback, job);
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#define bio_barrier(bio)		((bio)->bi_rw & (1 << BIO_RW_BARRIER))
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
#define bio_barrier(bio)		((bio)->bi_rw & REQ_HARDBARRIER)
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
#define bio_barrier(bio)		((bio)->bi_opf & REQ_PREFLUSH)
#else
#define bio_barrier(bio)		((bio)->bi_rw & REQ_FLUSH)
#endif
#endif
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
int rc_map(struct dm_target *ti, struct bio *bio)
#else
int rc_map(struct dm_target *ti, struct bio *bio, union map_info *map_context)
#endif
{
	struct cache_context *dmc = (struct cache_context *)ti->private;
	unsigned long flags;

	if (bio_barrier(bio))
		return -EOPNOTSUPP;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	ASSERT(to_sector(bio->bi_iter.bi_size) <= dmc->block_size);
#else
	ASSERT(to_sector(bio->bi_size) <= dmc->block_size);
#endif
	if (bio_data_dir(bio) == READ)
		dmc->reads++;
	else
		dmc->writes++;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
	if (to_sector(bio->bi_iter.bi_size) != dmc->block_size ||
#else
	if (to_sector(bio->bi_size) != dmc->block_size ||
#endif
	    (dmc->mode && (bio_data_dir(bio) == WRITE))) {

		spin_lock_irqsave(&dmc->cache_spin_lock, flags);
		(void)cache_invalidate_blocks(dmc, bio);
		spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
		rc_start_uncached_io(dmc, bio);
	} else {
		if (bio_data_dir(bio) == READ)
			cache_read(dmc, bio);
		else
			cache_write(dmc, bio);
	}
	return DM_MAPIO_SUBMITTED;
}
EXPORT_SYMBOL(rc_map);

static void rc_uncached_io_callback(unsigned long error, void *context)
{
	struct kcached_job *job = (struct kcached_job *)context;
	struct cache_context *dmc = job->dmc;
	unsigned long flags;

	spin_lock_irqsave(&dmc->cache_spin_lock, flags);
	if (bio_data_dir(job->bio) == READ)
		dmc->uncached_reads++;
	else
		dmc->uncached_writes++;
	(void)cache_invalidate_blocks(dmc, job->bio);
	spin_unlock_irqrestore(&dmc->cache_spin_lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
	bio_endio(job->bio, error);
#else
	if (error) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
		job->bio->bi_status = error;
#else
		job->bio->bi_error = error;
#endif
		bio_io_error(job->bio);
	} else {
		bio_endio(job->bio);
	}
#endif
	mempool_free(job, job_pool);
	if (atomic_dec_and_test(&dmc->nr_jobs))
		wake_up(&dmc->destroyq);
}

static void rc_start_uncached_io(struct cache_context *dmc, struct bio *bio)
{
	int is_write = (bio_data_dir(bio) == WRITE);
	struct kcached_job *job;

	job = new_kcached_job(dmc, bio, -1);
	if (unlikely(!job)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0)
		bio_endio(bio, -EIO);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
		bio->bi_status= -EIO;
#else
		bio->bi_error = -EIO;
#endif
		bio_io_error(bio);
#endif
		return;
	}
	atomic_inc(&dmc->nr_jobs);
	if (bio_data_dir(job->bio) == READ)
		dmc->disk_reads++;
	else
		dmc->disk_writes++;
	dm_io_async_bvec(1, &job->disk, ((is_write) ? WRITE : READ),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
			 bio, rc_uncached_io_callback, job);
#else
			 bio->bi_io_vec + bio->bi_idx, rc_uncached_io_callback, job);
#endif
}

static inline int rc_get_dev(struct dm_target *ti, char *pth,
			      struct dm_dev **dmd, char *dmc_dname,
			      sector_t tilen)
{
	int rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
	rc = dm_get_device(ti, pth, dm_table_get_mode(ti->table), dmd);
#else
#if defined(RHEL_MAJOR) && RHEL_MAJOR == 6
	rc = dm_get_device(ti, pth, dm_table_get_mode(ti->table), dmd);
#else 
	rc = dm_get_device(ti, pth, 0, tilen, dm_table_get_mode(ti->table), dmd);
#endif
#endif
	if (!rc)
		strncpy(dmc_dname, pth, DEV_PATHLEN);
	return rc;
}

/* Construct a cache mapping.
 *  arg[0]: path to source device
 *  arg[1]: path to cache device
 *  arg[2]: cache size (in blocks)
 *  arg[3]: mode: write through / around
 *  arg[4]: cache associativity */
static int cache_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct cache_context *dmc;
	unsigned int consecutive_blocks;
	sector_t i, order, tmpsize;
	sector_t data_size, dev_size;
	int r = -EINVAL;

	if (argc < 2) {
		ti->error = "rapiddisk-cache: Need at least 2 arguments";
		goto construct_fail;
	}

	dmc = kzalloc(sizeof(*dmc), GFP_KERNEL);
	if (!dmc) {
		ti->error = "rapiddisk-cache: Failed to allocate cache context";
		r = -ENOMEM;
		goto construct_fail;
	}

	dmc->tgt = ti;

	if (rc_get_dev(ti, argv[0], &dmc->disk_dev,
	    dmc->disk_devname, ti->len)) {
		ti->error = "rapiddisk-cache: Disk device lookup failed";
		goto construct_fail1;
	}
	if (strncmp(argv[1], "/dev/rd", 7) != 0) {
		pr_err("%s: %s is not a valid cache device for rapiddisk-cache.",
		       DM_MSG_PREFIX, argv[1]);
		ti->error = "rapiddisk-cache: Invalid cache device. Not a RapidDisk volume.";
		goto construct_fail2;
	}
	if (rc_get_dev(ti, argv[1], &dmc->cache_dev, dmc->cache_devname, 0)) {
		ti->error = "rapiddisk-cache: Cache device lookup failed";
		goto construct_fail2;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,39)
	dmc->io_client = dm_io_client_create();
#else
#if defined(RHEL_MAJOR) && RHEL_MAJOR == 6 && RHEL_MINOR >= 4
	dmc->io_client = dm_io_client_create();
#else
	dmc->io_client = dm_io_client_create(COPY_PAGES);
#endif
#endif
	if (IS_ERR(dmc->io_client)) {
		r = PTR_ERR(dmc->io_client);
		ti->error = "Failed to create io client\n";
		goto construct_fail3;
	}
#endif

	r = kcached_init(dmc);
	if (r) {
		ti->error = "Failed to initialize kcached";
		goto construct_fail4;
	}
	dmc->block_size = CACHE_BLOCK_SIZE;
	dmc->block_shift = ffs(dmc->block_size) - 1;
	dmc->block_mask = dmc->block_size - 1;

	if (argc >= 3) {
		if (kstrtoul(argv[2], 10, (unsigned long *)&dmc->size)) {
			ti->error = "rapiddisk-cache: Invalid cache size";
			r = -EINVAL;
			goto construct_fail5;
		}
	} else {
		dmc->size = to_sector(dmc->cache_dev->bdev->bd_inode->i_size);
	}

	if (argc >= 4) {
		if (sscanf(argv[3], "%d", &dmc->mode) != 1) {
			ti->error = "rapiddisk-cache: Invalid mode";
			r = -EINVAL;
			goto construct_fail5;
                }
	} else {
		dmc->mode = WRITETHROUGH;
	}

	if (argc >= 5) {
		if (kstrtoint(argv[4], 10, &dmc->assoc)) {
			ti->error = "rapiddisk-cache: Invalid cache associativity";
			r = -EINVAL;
			goto construct_fail5;
		}
		if (!dmc->assoc || (dmc->assoc & (dmc->assoc - 1)) ||
		    dmc->size < dmc->assoc) {
			ti->error = "rapiddisk-cache: Invalid cache associativity";
			r = -EINVAL;
			goto construct_fail5;
		}
	} else {
		dmc->assoc = DEFAULT_CACHE_ASSOC;
	}

	/* Convert size (in sectors) to blocks. Then round size
	 * (in blocks now) down to a multiple of associativity */
	do_div(dmc->size, dmc->block_size);
	tmpsize = dmc->size;
	do_div(tmpsize, dmc->assoc);
	dmc->size = tmpsize * dmc->assoc;

	dev_size = to_sector(dmc->cache_dev->bdev->bd_inode->i_size);
	data_size = dmc->size * dmc->block_size;
	if (data_size > dev_size) {
		DMERR("Requested cache size exceeds the cache device's capacity (%lu>%lu)",
		      (unsigned long)data_size, (unsigned long)dev_size);
		ti->error = "rapiddisk-cache: Invalid cache size";
		r = -EINVAL;
		goto construct_fail5;
	}

	consecutive_blocks = dmc->assoc;
	dmc->consecutive_shift = ffs(consecutive_blocks) - 1;

	order = dmc->size * sizeof(struct cache_block);
	DMINFO("Allocate %luKB (%luB per) mem for %lu-entry cache"
		"(capacity:%luMB, associativity:%u, block size:%u sectors(%uKB))",
		(unsigned long)order >> 10,
		(unsigned long)sizeof(struct cache_block),
		(unsigned long)dmc->size,
		(unsigned long)data_size >> (20 - SECTOR_SHIFT),
		dmc->assoc, dmc->block_size,
		dmc->block_size >> (10 - SECTOR_SHIFT));
	dmc->cache = vmalloc(order);
	if (!dmc->cache)
		goto construct_fail6;
	dmc->cache_state = vmalloc(dmc->size);
	if (!dmc->cache_state)
		goto construct_fail7;

	order = (dmc->size >> dmc->consecutive_shift) * sizeof(u32);
	dmc->set_lru_next = vmalloc(order);
	if (!dmc->set_lru_next)
		goto construct_fail8;

	for (i = 0; i < dmc->size ; i++) {
		dmc->cache[i].dbn = 0;
		dmc->cache_state[i] = INVALID;
	}

	/* Initialize the point where LRU sweeps begin for each set */
	for (i = 0 ; i < (dmc->size >> dmc->consecutive_shift) ; i++)
		dmc->set_lru_next[i] = i * dmc->assoc;

	spin_lock_init(&dmc->cache_spin_lock);

	dmc->reads = 0;
	dmc->writes = 0;
	dmc->cache_hits = 0;
	dmc->replace = 0;
	dmc->wr_invalidates = 0;
	dmc->rd_invalidates = 0;
	dmc->cached_blocks = 0;
	dmc->cache_wr_replace = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
	ti->split_io = dmc->block_size;
#else
	r = dm_set_target_max_io_len(ti, dmc->block_size);
	if (r)
		goto construct_fail8;
#endif
	ti->private = dmc;

	return 0;

construct_fail8:
	vfree(dmc->cache_state);
construct_fail7:
	vfree(dmc->cache);
construct_fail6:
	r = -ENOMEM;
	ti->error = "Unable to allocate memory";
construct_fail5:
	kcached_client_destroy(dmc);
construct_fail4:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
	dm_io_client_destroy(dmc->io_client);
#endif
construct_fail3:
	dm_put_device(ti, dmc->cache_dev);
construct_fail2:
	dm_put_device(ti, dmc->disk_dev);
construct_fail1:
	kfree(dmc);
construct_fail:
	return r;
}

static void cache_dtr(struct dm_target *ti)
{
	struct cache_context *dmc = (struct cache_context *) ti->private;

	kcached_client_destroy(dmc);

	if (dmc->reads + dmc->writes > 0) {
		DMINFO("stats:\n\treads(%lu), writes(%lu)\n",
		       dmc->reads, dmc->writes);
		DMINFO("\tcache hits(%lu), replacement(%lu), write replacement(%lu)\n"
			"\tread invalidates(%lu), write invalidates(%lu)\n",
			dmc->cache_hits, dmc->replace, dmc->cache_wr_replace,
			dmc->rd_invalidates, dmc->wr_invalidates);
		DMINFO("conf:\n\tcapacity(%luM), associativity(%u), block size(%uK)\n"
			"\ttotal blocks(%lu), cached blocks(%lu)\n",
			(unsigned long)dmc->size * dmc->block_size >> 11,
			dmc->assoc, dmc->block_size >> (10 - SECTOR_SHIFT),
			(unsigned long)dmc->size, dmc->cached_blocks);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
	dm_io_client_destroy(dmc->io_client);
#endif
	vfree(dmc->cache);
	vfree(dmc->cache_state);
	vfree(dmc->set_lru_next);

	dm_put_device(ti, dmc->disk_dev);
	dm_put_device(ti, dmc->cache_dev);
	kfree(dmc);
}

static void rc_status_info(struct cache_context *dmc, status_type_t type,
			    char *result, unsigned int maxlen)
{
	int sz = 0;

	DMEMIT("stats:\n\treads(%lu), writes(%lu)\n", dmc->reads, dmc->writes);
	DMEMIT("\tcache hits(%lu), replacement(%lu), write replacement(%lu)\n"
		"\tread invalidates(%lu), write invalidates(%lu)\n"
		"\tuncached reads(%lu), uncached writes(%lu)\n"
		"\tdisk reads(%lu), disk writes(%lu)\n"
		"\tcache reads(%lu), cache writes(%lu)\n",
		dmc->cache_hits, dmc->replace, dmc->cache_wr_replace,
		dmc->rd_invalidates, dmc->wr_invalidates,
		dmc->uncached_reads, dmc->uncached_writes,
		dmc->disk_reads, dmc->disk_writes,
		dmc->cache_reads, dmc->cache_writes);
}

static void rc_status_table(struct cache_context *dmc, status_type_t type,
			     char *result, unsigned int maxlen)
{
	int sz = 0;

	DMEMIT("conf:\n\tRapidDisk dev (%s), disk dev (%s) mode (%s)\n"
		"\tcapacity(%luM), associativity(%u), block size(%uK)\n"
		"\ttotal blocks(%lu), cached blocks(%lu)\n",
		dmc->cache_devname, dmc->disk_devname,
		((dmc->mode) ? "WRITE_AROUND" : "WRITETHROUGH"),
		(unsigned long)dmc->size * dmc->block_size >> 11, dmc->assoc,
		dmc->block_size >> (10 - SECTOR_SHIFT),
		(unsigned long)dmc->size, dmc->cached_blocks);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,3)
static int
#else
static void
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
cache_status(struct dm_target *ti, status_type_t type, char *result,
	     unsigned int maxlen)
#else
cache_status(struct dm_target *ti, status_type_t type, unsigned status_flags,
	     char *result, unsigned int maxlen)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	int sz = 0;
#endif
	struct cache_context *dmc = (struct cache_context *)ti->private;

	switch (type) {
	case STATUSTYPE_INFO:
		rc_status_info(dmc, type, result, maxlen);
		break;
	case STATUSTYPE_TABLE:
		rc_status_table(dmc, type, result, maxlen);
		break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	case STATUSTYPE_IMA:
		DMEMIT_TARGET_NAME_VERSION(ti->type);
		DMEMIT(",source_device_name=%s,cache_device_name=%s;", dmc->disk_dev->name,
		       dmc->cache_dev->name);
		break;
#endif
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,3)
	return 0;
#endif
}

static struct target_type cache_target = {
	.name    = "rapiddisk-cache",
	.version = {8, 2, 0},
	.module  = THIS_MODULE,
	.ctr	 = cache_ctr,
	.dtr	 = cache_dtr,
	.map	 = rc_map,
	.status  = cache_status,
};

int __init rc_init(void)
{
	int ret;

	ret = jobs_init();
	if (ret)
		return ret;

	kcached_wq = create_singlethread_workqueue("kcached");
	if (!kcached_wq)
		return -ENOMEM;

	INIT_WORK(&kcached_work, do_work);

	ret = dm_register_target(&cache_target);
	if (ret < 0)
		return ret;
	return 0;
}

void rc_exit(void)
{
	dm_unregister_target(&cache_target);
	jobs_exit();
	destroy_workqueue(kcached_wq);
}

module_init(rc_init);
module_exit(rc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Petros Koutoupis <petros@petroskoutoupis.com>");
MODULE_DESCRIPTION("RapidDisk-Cache DM target is a write-through caching target with RapidDisk volumes.");
MODULE_VERSION(VERSION_STR);
MODULE_INFO(Copyright, "Copyright 2010 - 2022 Petros Koutoupis");
