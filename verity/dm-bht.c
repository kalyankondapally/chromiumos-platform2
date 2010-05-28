 /*
 * Copyright (C) 2010 The Chromium OS Authors <chromium-os-dev@chromium.org>
 *
 * Device-Mapper block hash tree interface.
 * See Documentation/device-mapper/dm-bht.txt for details.
 *
 * This file is released under the GPL.
 */

#include <asm/atomic.h>
#include <asm/bug.h>
#include <asm/errno.h>
#include <asm/page.h>
#include <linux/bitops.h>  /* for fls() */
#include <linux/cpumask.h>  /* nr_cpu_ids */
/* #define CONFIG_DM_DEBUG 1 */
#include <linux/device-mapper.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/dm-bht.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>  /* k*alloc */
#include <linux/string.h>  /* memset */

#define DM_MSG_PREFIX "dm bht"

/* For sector formatting. */
#if defined (_LP64) || defined (__LP64__) || __BITS_PER_LONG == 64
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX "ll"
#endif
#define PRIu64 __PRIS_PREFIX "u"


/*-----------------------------------------------
 * Utilities
 *-----------------------------------------------*/

static u8 from_hex(u8 ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'A' + 10;
	return -1;
}

/**
 * dm_bht_bin_to_hex - converts a binary stream to human-readable hex
 * @binary:	a byte array of length @binary_len
 * @hex:	a byte array of length @binary_len * 2 + 1
 */
static void dm_bht_bin_to_hex(u8 *binary, u8 *hex, u32 binary_len)
{
	while (binary_len-- > 0) {
		sprintf((char * __restrict__)hex, "%02hhx", (int)*binary);
		hex += 2;
		binary++;
	}
}

/**
 * dm_bht_hex_to_bin - converts a hex stream to binary
 * @binary:	a byte array of length @binary_len
 * @hex:	a byte array of length @binary_len * 2 + 1
 */
static void dm_bht_hex_to_bin(u8 *binary, const u8 *hex, u32 binary_len)
{
	while (binary_len-- > 0) {
		*binary = from_hex(*(hex++));
		*binary *= 16;
		*binary += from_hex(*(hex++));
		binary++;
	}
}

static void dm_bht_log_mismatch(struct dm_bht *bht, u8 *given, u8 *computed)
{
	u8 given_hex[DM_BHT_MAX_DIGEST_SIZE * 2 + 1];
	u8 computed_hex[DM_BHT_MAX_DIGEST_SIZE * 2 + 1];
	dm_bht_bin_to_hex(given, given_hex, bht->digest_size);
	dm_bht_bin_to_hex(computed, computed_hex, bht->digest_size);
	DMERR_LIMIT("%s != %s", given_hex, computed_hex);
}

/*-----------------------------------------------
 * Implementation functions
 *-----------------------------------------------*/

static int dm_bht_initialize_entries(struct dm_bht *bht);

static int dm_bht_read_callback_stub(void *ctx, sector_t start, u8 *dst,
				     sector_t count,
				     struct dm_bht_entry *entry);
static int dm_bht_write_callback_stub(void *ctx, sector_t start,
				      u8 *dst, sector_t count,
				      struct dm_bht_entry *entry);

/**
 * dm_bht_create - prepares @bht for us
 * @bht:	pointer to a dm_bht_create()d bht
 * @depth:	tree depth without the root; including block hashes
 * @block_count:the number of block hashes / tree leaves
 * @alg_name:	crypto hash algorithm name
 *
 * Returns 0 on success.
 *
 * Callers can offset into devices by storing the data in the io callbacks.
 * TODO(wad) bust up into smaller helpers
 */
int dm_bht_create(struct dm_bht *bht, unsigned int depth, u32 block_count,
		  const char *alg_name)
{
	int status = 0;
	int cpu = 0;
	BUG_ON(!bht);

	/* Allocate enough crypto contexts to be able to perform verifies
	 * on all available CPUs */
	bht->hash_desc = (struct hash_desc *)
		kcalloc(nr_cpu_ids, sizeof(struct hash_desc), GFP_KERNEL);
	if (!bht->hash_desc) {
		DMERR("failed to allocate crypto hash contexts");
		return -ENOMEM;
	}

	/* Setup the hash first. Its length determines much of the bht layout */
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu) {
		bht->hash_desc[cpu].tfm = crypto_alloc_hash(alg_name, 0, 0);
		if (IS_ERR(bht->hash_desc[cpu].tfm)) {
			DMERR("failed to allocate crypto hash '%s'", alg_name);
			status = -ENOMEM;
			bht->hash_desc[cpu].tfm = NULL;
			goto bad_hash_alg;
		}
	}
	bht->digest_size = crypto_hash_digestsize(bht->hash_desc[0].tfm);
	/* We expect to be able to pack >=2 hashes into a page */
	if (PAGE_SIZE / bht->digest_size < 2) {
		DMERR("too few hashes fit in a page");
		status = -EINVAL;
		goto bad_digest_len;
	}

	if (bht->digest_size > DM_BHT_MAX_DIGEST_SIZE) {
		DMERR("DM_BHT_MAX_DIGEST_SIZE too small for chosen digest");
		status = -EINVAL;
		goto bad_digest_len;
	}
	bht->root_digest = (u8 *)kzalloc(bht->digest_size, GFP_KERNEL);
	if (!bht->root_digest) {
		DMERR("failed to allocate memory for root digest");
		status = -ENOMEM;
		goto bad_root_digest_alloc;
	}
	bht->root_verified = false;

	/* Configure the tree */
	bht->block_count = block_count;
	DMDEBUG("Setting block_count %u", block_count);
	if (block_count == 0) {
		DMERR("block_count must be non-zero");
		status = -EINVAL;
		goto bad_block_count;
	}

	bht->depth = depth;  /* assignment below */
	DMDEBUG("Setting depth %u", depth);
	if (!depth || depth > UINT_MAX / sizeof(struct dm_bht_level)) {
		DMERR("bht depth is invalid: %u", depth);
		status = -EINVAL;
		goto bad_depth;
	}

	/* Allocate levels. Each level of the tree may have an arbitrary number
	 * of dm_bht_entry structs.  Each entry contains node_count nodes.
	 * Each node in the tree is a cryptographic digest of either node_count
	 * nodes on the subsequent level or of a specific block on disk. */
	bht->levels = (struct dm_bht_level *)
		        kcalloc(depth, sizeof(struct dm_bht_level), GFP_KERNEL);
	if (!bht->levels) {
		DMERR("failed to allocate tree levels");
		status = -ENOMEM;
		goto bad_level_alloc;
	}

	/* Each dm_bht_entry->nodes is one page.  The node code tracks
	 * how many nodes fit into one entry where a node is a single
	 * hash (message digest). */
	bht->node_count_shift = fls(PAGE_SIZE / bht->digest_size) - 1;
	/* Round down to the nearest power of two.  This makes indexing
	 * into the tree much less painful. */
	bht->node_count = 1 << bht->node_count_shift;

	/* This is unlikely to happen, but with 64k pages, who knows. */
	if (bht->node_count > UINT_MAX / bht->digest_size) {
		DMERR("node_count * hash_len exceeds UINT_MAX!");
		status = -EINVAL;
		goto bad_node_count;
	}
	/* Ensure that we can safely shift by this value. */
	if (depth * bht->node_count_shift >= sizeof(u32) * 8) {
		DMERR("specified depth and node_count_shift is too large");
		status = -EINVAL;
		goto bad_node_count;
	}

	/* Setup callback stubs */
	bht->read_cb = &dm_bht_read_callback_stub;
	bht->write_cb = &dm_bht_write_callback_stub;

	status = dm_bht_initialize_entries(bht);
	if (status)
		goto bad_entries_alloc;

	return 0;

bad_entries_alloc:
	while (bht->depth-- > 0) {
		if (bht->levels[bht->depth].entries)
			kfree(bht->levels[bht->depth].entries);
	}
bad_node_count:
	kfree(bht->levels);
bad_level_alloc:
bad_block_count:
bad_depth:
	kfree(bht->root_digest);
bad_root_digest_alloc:
bad_digest_len:
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
		if (bht->hash_desc[cpu].tfm)
			crypto_free_hash(bht->hash_desc[cpu].tfm);
bad_hash_alg:
	kfree(bht->hash_desc);
	return status;
}
EXPORT_SYMBOL_GPL(dm_bht_create);

static int dm_bht_initialize_entries(struct dm_bht *bht)
{
	/* Walk the tree and allocate by considering the last possible
	 * child assuming that the tree is fully populated.  Any gaps
	 * MUST be padded on disk.
	 * Note, we treat both the tree root (1 hash) and the tree leaves
	 * as external to the level data.  The root is depth=-1 and the block
	 * layer level is depth=bht->depth */
	u32 last_index = ALIGN(bht->block_count, bht->node_count) - 1;
	u32 total_entries = 0;
	struct dm_bht_level *level = NULL;
	unsigned int depth = 0;

	/* check that the largest level->count can't result in an int overflow
	 * on allocation or sector calculation. */
	if (((last_index >> bht->node_count_shift) + 1) >
	    UINT_MAX / max((u32)sizeof(struct dm_bht_entry),
			   (u32)to_sector(PAGE_SIZE))) {
		DMCRIT("required entries %u is too large",
		       last_index + 1);
		return -EINVAL;
	}

	/* Track the current sector location for each level so we don't have to
	 * compute it during traversals. */	
	bht->sectors = 0;
	do {
		u32 shift = (bht->depth - depth) * bht->node_count_shift;
		level = &bht->levels[depth];
		/* Depth is subtracted so that we can calculate the offset
		 * treating the root as depth=0 for easier
		 * debugging/comprehension */
		level->count = (last_index >> shift) + 1;
		DMDEBUG("depth: %u entries: %u", depth, level->count);
		/* TODO(wad) could add entry_node_count and pre-allocate the
		 * data but and make the entry->data =
		 * &level->entries[level->count] + entry_index; It's more ideal
		 * to either allocate so it can be freed indep (via mempool) or
		 * use a LRU cache and when kicked, bump the entry back to
		 * UNALLOCATED.  Right now, the state transition doesn't
		 * support that but it could
		 */
		level->entries = (struct dm_bht_entry *) kcalloc(level->count,
				   sizeof(struct dm_bht_entry), GFP_KERNEL);
		total_entries += level->count;
		if (!level->entries) {
			DMERR("failed to allocate entries for depth %u",
			      bht->depth);
			/* let the caller clean up the mess */
			return -ENOMEM;
		}
		level->sector = bht->sectors;
		/* number of sectors per entry * entries at this level */
		bht->sectors += level->count * to_sector(PAGE_SIZE);
		/* not ideal, but since unsigned overflow behavior is defined */
		if (bht->sectors < level->sector) {
			DMCRIT("level sector calculation overflowed");
			return -EINVAL;
		}
	} while (++depth < bht->depth);  /* Deepest layer is the block dev */

	/* Go ahead and reserve enough space for everything.  We really don't
	 * want memory allocation failures.  Once we start freeing verified
	 * entries, then we can reduce this reservation. */ 
	bht->entry_pool = mempool_create_page_pool(total_entries, 0);
	if (!bht->entry_pool) {
		DMERR("failed to allocate mempool");
		return -ENOMEM;
	}
			
	return 0;
}

static int dm_bht_read_callback_stub(void *ctx, sector_t start, u8 *dst,
				     sector_t count, struct dm_bht_entry *entry)
{
	DMCRIT("dm_bht_read_callback_stub called!");
	dm_bht_read_completed(entry, -EIO);
	return -EIO;
}

static int dm_bht_write_callback_stub(void *ctx, sector_t start,
				      u8 *dst, sector_t count,
				      struct dm_bht_entry *entry)
{
	DMCRIT("dm_bht_write_callback_stub called!");
	dm_bht_write_completed(entry, -EIO);
	return -EIO;
}

/**
 * dm_bht_read_completed
 * @entry:	pointer to the entry that's been loaded
 * @status:	I/O status. Non-zero is failure.
 * MUST always be called after a read_cb completes.
 */
void dm_bht_read_completed(struct dm_bht_entry *entry, int status)
{
	int state;
	BUG_ON(!entry);
	if (status) {
		/* TODO(wad) add retry support */
		DMCRIT("an I/O error occurred while reading entry");
		atomic_set(&entry->state, DM_BHT_ENTRY_ERROR_IO);
		/* entry->nodes will be freed later */
		return;
	}
	state = atomic_cmpxchg(&entry->state,
				   DM_BHT_ENTRY_PENDING,
				   DM_BHT_ENTRY_READY);
	if (state != DM_BHT_ENTRY_PENDING) {
		DMCRIT("state changed on entry out from under io");
		BUG();
	}
}
EXPORT_SYMBOL_GPL(dm_bht_read_completed);

/**
 * dm_bht_write_completed
 * @entry:	pointer to the entry that's been loaded
 * @status:	I/O status. Non-zero is failure.
 * Should be called after a write_cb completes. Currently only catches
 * errors which more writers don't care about.
 */
void dm_bht_write_completed(struct dm_bht_entry *entry, int status)
{
	BUG_ON(!entry);
	if (status) {
		DMCRIT("an I/O error occurred while writing entry");
		atomic_set(&entry->state, DM_BHT_ENTRY_ERROR_IO);
		/* entry->nodes will be freed later */
		return;
	}
}
EXPORT_SYMBOL_GPL(dm_bht_write_completed);


/* dm_bht_maybe_read_entries
 * Attempts to atomically acquire each entry, allocated any needed
 * memory, and issues I/O callbacks to load the hashes from disk.
 * Returns 0 if all entries are loaded and verified.  On error, the
 * return value is negative. When positive, it is the state values
 * ORd.
 */
static int dm_bht_maybe_read_entries(struct dm_bht *bht, void *ctx,
				     unsigned int depth, u32 index,
				     u32 count)
{
	struct dm_bht_level *level;
	struct dm_bht_entry *entry, *last_entry;
	sector_t current_sector;
	int state = 0;
	int status = 0;
	struct page *node_page = NULL;
	/* XXX: replace with DM_BHT_BUG_ON */
	BUG_ON(!bht);
	BUG_ON(depth >= bht->depth);

	level = &bht->levels[depth];
	if (count > level->count - index) {
		DMERR("dm_bht_maybe_read_entries(%u,%u,%u): "
		      "index+count exceeds available entries %u",
			depth, index, count, level->count);
		return -EINVAL;
	}
	/* XXX: hardcoding PAGE_SIZE means that a perfectly valid image
	 *      on one system may not work on a different kernel.
	 * TODO(wad) abstract PAGE_SIZE with a bht->entry_size or
	 *           at least a define and ensure bht->entry_size is
	 *           sector aligned at least. */
	current_sector = level->sector + to_sector(index * PAGE_SIZE);
	for (entry = &level->entries[index], last_entry = entry + count;
	     entry < last_entry;
	     ++entry, current_sector += to_sector(PAGE_SIZE)) {
		/* If the entry's state is UNALLOCATED, then we'll claim it
		 * for allocation and loading */
		state = atomic_cmpxchg(&entry->state,
				       DM_BHT_ENTRY_UNALLOCATED,
				       DM_BHT_ENTRY_PENDING);
		DMDEBUG("dm_bht_maybe_read_entries(%u,%u,%u): "
			"ei=%lu, state=%d",
			depth, index, count,
			(unsigned long)(entry - level->entries), state);
		if (state <= DM_BHT_ENTRY_ERROR) {
			DMCRIT("entry %u is in an error state", index);
			return state;
		}

		if (state == DM_BHT_ENTRY_VERIFIED) {
			/* Makes 0 == verified. Is that ideal? */
			continue;
		}

		if (state != DM_BHT_ENTRY_UNALLOCATED) {
			/* PENDING, READY, ... */
			status |= state;
			continue;
		}
		/* Current entry is claimed for allocation and loading */
		node_page = (struct page *) mempool_alloc(bht->entry_pool,
							  GFP_NOIO);
		if (!node_page) {
			DMCRIT("failed to allocate memory for "
			       "entry->nodes from pool");
			return -ENOMEM;
		}
		/* dm-bht guarantees page-aligned memory for callbacks. */
		entry->nodes = page_address(node_page);
		/* Let the caller know that not all the data is yet available */
		status |= DM_BHT_ENTRY_REQUESTED;
		/* Issue the read callback */
		/* TODO(wad) error check callback here too */
		bht->read_cb(ctx,   /* external context */
			     current_sector,  /* starting sector */
			     entry->nodes,  /* destination */
			     to_sector(PAGE_SIZE),
			     entry);  /* io context */

	}
	/* Should only be 0 if all entries were verified and not just ready */
	return status;
}

/* Used for turning verifiers into computers */
typedef int (*dm_bht_compare_cb)(struct dm_bht *, u8 *, u8 *);

static int dm_bht_compare_hash(struct dm_bht *bht, u8 *known, u8 *computed)
{
	return memcmp(known, computed, bht->digest_size);
}		

static int dm_bht_update_hash(struct dm_bht *bht, u8 *known, u8 *computed)
{
	memcpy(known, computed, bht->digest_size);
	return 0;
}		

/**
 * dm_bht_verify_entry - computes the digest of each entry and compares it
 * @bht:	pointer to a dm_bht_create()d bht
 * @hashes:	array of expected hash values of each nodes
 * @entries:	array of entries whose HASH(entry->nodes) is computed
 * @count:	the number of elements in @entries and @hashes
 * @compare_cb:	callback which compares the computed hash to the expected hash
 *
 * @hashes is usually the entry->nodes value of the parent of the @entries
 * array supplied.
 *
 * In the normal case, HASH(entry->nodes) is compared against
 * @hashes[entry_index], but compare_cb may be used to populate the values in
 * @hashes.
 */
static int dm_bht_verify_entry(struct dm_bht *bht, u8 *hashes,
			       struct dm_bht_entry *entries, unsigned int count,
			       dm_bht_compare_cb compare_cb)
{
	struct hash_desc *hash_desc;
	struct scatterlist sg;  /* feeds digest() */
	struct dm_bht_entry *entry = entries;
	int state = 0;
	u8 *end = NULL;
	u8 digest[DM_BHT_MAX_DIGEST_SIZE];
	/* Basic sanity checking */
	BUG_ON(!bht);
	BUG_ON(!bht->digest_size);
	BUG_ON(!hashes);
	BUG_ON(!entries);
	if (count > bht->node_count) {
		DMERR("dm_bht_verify_entry called with count > node_count");
		return -EINVAL;
	}
	/* Grab the hash that is reserved for our cpu wq */
	hash_desc = &bht->hash_desc[smp_processor_id()];
	for (end=hashes + (bht->digest_size * count);
	     hashes < end;
	     hashes += bht->digest_size, ++entry) {
		/* Check if target entry is loaded. */
		state = atomic_read(&entry->state);
		if (state <= DM_BHT_ENTRY_ERROR) {
			DMERR("entry marked bad before verify: %u of %u",
			      (u32)(entry - entries), count);
			return state;
		}
		if (state <= DM_BHT_ENTRY_PENDING) {
			DMERR("entry not ready for verify: %u of %u: %d",
			      (u32)(entry - entries), count, state);
			return 1;
		}
		/* Note, we don't check if the entry itself has been verified.
		 * All that we care about that each hash in the hashes array
		 * are all correct. */
		sg_init_table(&sg, 1);
		sg_set_page(&sg, virt_to_page(entry->nodes), PAGE_SIZE, 0);
		/* Note, this is synchronous. */
		if (crypto_hash_init(hash_desc)) {
			DMCRIT("failed to reinitialize crypto hash (proc:%d)",
				smp_processor_id());
			return -EINVAL;
		}
		if (crypto_hash_digest(hash_desc, &sg, PAGE_SIZE, digest)) {
			DMCRIT("crypto_hash_digest failed");
			return -EINVAL;
		}
		if (compare_cb(bht, hashes, digest)) {
			DMERR("entry failed to validate: %u of %u",
			      (u32)(entry - entries), count);
			return DM_BHT_ENTRY_ERROR_MISMATCH;
		}
	}
	return 0;
}

/* digest length and bht must be checked already */
static int dm_bht_check_block(struct dm_bht *bht, u32 block_index,
			      u8 *digest)
{
	int depth;
	u32 index;
	struct dm_bht_entry *entry;
	int state;

	/* The leaves contain the block hashes */
	depth = bht->depth - 1;

	/* Index into the bottom level.  Each entry in this level contains
	 * nodes whose hashes are the direct hashes of one block of data on
	 * disk. */
	index = block_index >> bht->node_count_shift;
	entry = &bht->levels[depth].entries[index];

	state = atomic_read (&entry->state);
	if (state <= DM_BHT_ENTRY_ERROR) {
		DMCRIT("leaf entry for block %u is invalid",
		       block_index);
		return state;
	}
	if (state <= DM_BHT_ENTRY_PENDING) {
		DMERR("leaf data not yet loaded for block %u",
		      block_index);
		return 1;
	}

	/* Index into the entry data */
	index = (block_index % bht->node_count) * bht->digest_size;
	if (memcmp(&entry->nodes[index], digest, bht->digest_size)) {
		DMCRIT("digest mismatch for block %u", block_index);
		dm_bht_log_mismatch(bht, &entry->nodes[index], digest);
		return DM_BHT_ENTRY_ERROR_MISMATCH;
	}
	/* TODO(wad) update bht->block_bitmap here or in the caller */
	return 0;
}

/* Walk all entries at level 0 to compute the root digest.
 * 0 on success */
static int dm_bht_compute_root(struct dm_bht *bht, u8 *digest)
{
	struct dm_bht_entry *entry;
	u32 count;
	struct scatterlist sg;  /* feeds digest() */
	struct hash_desc *hash_desc;
	BUG_ON(!bht);
	hash_desc = &bht->hash_desc[smp_processor_id()];
	entry = bht->levels[0].entries;

        if (crypto_hash_init(hash_desc)) {
		DMCRIT("failed to reinitialize crypto hash (proc:%d)",
			smp_processor_id());
		return -EINVAL;
	}

	/* Point the scatterlist to the entries, then compute the digest */
	for (count = 0; count < bht->levels[0].count; ++count, ++entry) {
		if (atomic_read(&entry->state) <= DM_BHT_ENTRY_PENDING) {
			DMCRIT("data not ready to compute root: %u",
			       count);
			return 1;
		}
		sg_init_table(&sg, 1);
		sg_set_page(&sg, virt_to_page(entry->nodes), PAGE_SIZE, 0);
		if (crypto_hash_update(hash_desc, &sg, PAGE_SIZE)) {
			DMCRIT("Failed to update crypto hash");
			return -EINVAL;
		}
	}

	if (crypto_hash_final(hash_desc, digest)) {
		DMCRIT("Failed to compute final digest");
		return -EINVAL;
	}

	return 0;
}

static int dm_bht_verify_root(struct dm_bht *bht,
			      dm_bht_compare_cb compare_cb)
{
	int status = 0;
	u8 digest[DM_BHT_MAX_DIGEST_SIZE];
	BUG_ON(!bht);
	if (bht->root_verified)
		return 0;
	if ((status = dm_bht_compute_root(bht, digest))) {
		DMCRIT("Failed to compute root digest for verification");
		return status;
	}
	DMDEBUG("root computed");
	if ((status = compare_cb(bht, bht->root_digest, digest))) {
		DMCRIT("invalid root digest: %d", status);
		dm_bht_log_mismatch(bht, bht->root_digest, digest);
		return DM_BHT_ENTRY_ERROR_MISMATCH;
	}
	bht->root_verified = true;
	return 0;
}

/* dm_bht_verify_levels
 * Walks levels up to bht->depth - 2 and verifies the intermediary
 * node hashes */
static int dm_bht_verify_levels(struct dm_bht *bht, u32 block_index,
				dm_bht_compare_cb compare_cb)
{
	unsigned int depth = 0;
	u32 entry_index;
	struct dm_bht_level *level;
	struct dm_bht_entry *entry;
	struct dm_bht_entry *parent = NULL;
	int state;
	unsigned int verify_count = 0;
	bool parent_verified = true;
	unsigned int node_mask = 0;
	int verified = 0;
	u8 *hashes = NULL;

	/* The tree is then walked from 0 to bht->depth-2 to verify any
	 * intermediate nodes in the tree */
	node_mask = bht->node_count - 1;  /* pre-computed for easy masking */
	do {
		level = &bht->levels[depth];
		entry_index = block_index >>
			      ((bht->depth - depth) * bht->node_count_shift);
		DMDEBUG("verify_levels for bi=%u on d=%d ei=%u (max=%u)",
			block_index, depth, entry_index, level->count);
		/* XXX: BUG_ON(!level->entries) */
		entry = &level->entries[entry_index];
		state = atomic_read(&entry->state);

		if (state <= DM_BHT_ENTRY_ERROR) {
			DMCRIT("entry(d=%u,idx=%u) is in an error state: %d",
			       depth, entry_index, state);
			DMCRIT("verification is not possible");
			return state;
		} else if (state <= DM_BHT_ENTRY_PENDING) {
			DMERR("entry not ready for verify: d=%u,e=%u",
			      depth, entry_index);
			return 1;
		}

		/* If the parent is verified, then there's no work to do
		 * at this level. */
		if (parent_verified) {
			if (state != DM_BHT_ENTRY_VERIFIED) {
				parent = entry;
				parent_verified = false;
			}
			continue;
		}
		/* The parent was not verified, so we need to walk it */
		entry_index &= (~(node_mask));
		verify_count = min((u32)bht->node_count,
				   level->count - entry_index);
		DMDEBUG("verify_entry for %u [%u]",
			entry_index, bht->node_count);

		hashes = parent->nodes;
		verified = dm_bht_verify_entry(bht,
					       hashes,
					       level->entries + entry_index,
					       verify_count,
					       compare_cb);
		/* TODO(wad) make a generic dm_bht_error_check() */
		if (verified != 0) {
			DMERR("failed to verify (d=%u,base=%u)",
			      depth, entry_index);
			return verified;
		}
		/* Transition parent to verified. Avoid using _set
		 * in case we allow any other transitions from ready. */
		atomic_cmpxchg(&parent->state,
			       DM_BHT_ENTRY_READY,
			       DM_BHT_ENTRY_VERIFIED);
		parent = entry;
		parent_verified = (state == DM_BHT_ENTRY_VERIFIED);
	} while (++depth < bht->depth);  /* XXX ?? */
	return verified;
}

/**
 * dm_bht_store_block - sets a given block's hash in the tree
 * @bht:	pointer to a dm_bht_create()d bht
 * @block_index:numeric index of the block in the tree
 * @digest:	array of u8s containing the digest of length @bht->digest_size
 *
 * Returns 0 on success, >0 when data is pending, and <0 when a IO or other
 * error has occurred.
 *
 * If the containing entry in the tree is unallocated, it will allocate memory
 * and mark the entry as ready.  All other block entries will be 0s.  This
 * function is not safe for simultaneous use when verifying data and should not
 * be used if the @bht is being accessed by any other functions in any other
 * threads/processes.
 */
int dm_bht_store_block(struct dm_bht *bht, u32 block_index,
		       u8 *block_data)
{
	int depth;
	u32 index;
	struct dm_bht_entry *entry;
	int state;
	struct scatterlist sg;
	struct hash_desc *hash_desc;
	u8 digest[DM_BHT_MAX_DIGEST_SIZE];
	struct page *node_page = NULL;

	/* The leaves contain the block hashes */
	depth = bht->depth - 1;

	/* Index into the level */
	index = block_index >> bht->node_count_shift;
	entry = &bht->levels[depth].entries[index];
	DMDEBUG("Storing block %u in d=%d,ei=%u,s=%d",
		block_index, depth, index, atomic_read(&entry->state));

	state = atomic_cmpxchg(&entry->state,
			       DM_BHT_ENTRY_UNALLOCATED,
			       DM_BHT_ENTRY_PENDING);
	/* !!! Note. It is up to the users of the update interface to
	 *     ensure the entry data is fully populated prior to use.
	 *     The number of updated entries is NOT tracked. */
	if (state == DM_BHT_ENTRY_UNALLOCATED) {
		node_page = (struct page *) mempool_alloc(bht->entry_pool,
							  GFP_KERNEL);
		if (!node_page) {
			atomic_set(&entry->state, DM_BHT_ENTRY_ERROR);
			return -ENOMEM;
		}
		entry->nodes = page_address(node_page);
		/* TODO(wad) could expose this to the caller to that they
		 * can transition from unallocated to ready manually. */
		atomic_set(&entry->state, DM_BHT_ENTRY_READY);
	} else if (state <= DM_BHT_ENTRY_ERROR) {
		DMCRIT("leaf entry for block %u is invalid",
		      block_index);
		return state;
	} else if (state == DM_BHT_ENTRY_PENDING) {
		DMERR("leaf data is pending for block %u", block_index);
		return 1;
	}

	/* Compute the hash for the given page of data */
	hash_desc = &bht->hash_desc[smp_processor_id()];
	sg_init_table(&sg, 1);
	sg_set_buf(&sg, block_data, PAGE_SIZE);
	if (crypto_hash_init(hash_desc)) {
		DMCRIT("failed to reinitialize crypto hash (proc:%d)",
			smp_processor_id());
		return -EINVAL;
	}
	if (crypto_hash_digest(hash_desc, &sg, PAGE_SIZE, digest)) {
		DMCRIT("crypto_hash_digest failed");
		return -EINVAL;
	}

	/* Index into the entry data */
	index = (block_index % bht->node_count) * bht->digest_size;
	DMDEBUG("Storing block %u in node offset %u",
		block_index, index);
	memcpy(&entry->nodes[index], digest, bht->digest_size);
	return 0;
}
EXPORT_SYMBOL_GPL(dm_bht_store_block);

/**
 * dm_bht_zeroread_callback - read callback which always returns 0s
 * @ctx:	ignored
 * @start:	ignored
 * @data:	buffer to write 0s to
 * @count:	number of sectors worth of data to write
 * @complete_ctx: opaque context for @completed
 * @completed: callback to confirm end of data read
 *
 * Always returns 0.
 *
 * Meant for use by dm_compute() callers.  It allows dm_populate to
 * be used to pre-fill a tree with zeroed out entry nodes.
 */
int dm_bht_zeroread_callback(void *ctx, sector_t start, u8 *dst,
			     sector_t count, struct dm_bht_entry *entry)
{
	memset(dst, 0, to_bytes(count));
	dm_bht_read_completed(entry, 0);
	return 0;
}
EXPORT_SYMBOL_GPL(dm_bht_zeroread_callback);

/**
 * dm_bht_compute - computes and updates all non-block-level hashes in a tree
 * @bht:	pointer to a dm_bht_create()d bht
 * @read_cb_ctx:opaque read_cb context for all I/O on this call
 *
 * Returns 0 on success, >0 when data is pending, and <0 when a IO or other
 * error has occurred.
 *
 * Walks the tree and computes the hashes at each level from the
 * hashes below. This can only be called once per tree creation
 * since it will mark entries verified. Expects dm_bht_populate() to
 * correctly populate the tree from the read_callback_stub.
 *
 * This function should not be used when verifying the same tree and
 * should not be used with multiple simultaneous operators on @bht.
 */
int dm_bht_compute(struct dm_bht *bht, void *read_cb_ctx)
{
	u32 block;
	int updated = 0;
	BUG_ON(!bht);
	/* Start in the last entry block aligned so we can sub node_count */
	block = (bht->block_count - 1) & (~(bht->node_count - 1));
	/* Call verify levels once per node_count blocks */
	do {
		DMDEBUG("Updating levels for block %u", block);
		updated = dm_bht_populate(bht, read_cb_ctx, block);
		if (updated < 0) {
			DMERR("Failed to pre-zero entries");
			return updated;
		}
		updated = dm_bht_verify_levels(bht,
					       block,
					       dm_bht_update_hash);
		if (updated) {
			DMERR("Failed to update levels for block %u",
			      block);
			return updated;
		}
		if (bht->node_count >= block) {
			break;
		}
		block -= bht->node_count;
	} while (block > 0);
	/* Don't forget the root digest! */
	DMDEBUG("Calling verify_root with update_hash");
	updated = dm_bht_verify_root(bht, dm_bht_update_hash);
	return updated;
}
EXPORT_SYMBOL_GPL(dm_bht_compute);

/**
 * dm_bht_sync - writes the tree in memory to disk
 * @bht:	pointer to a dm_bht_create()d bht
 * @write_ctx:	callback context for writes issued
 *
 * Since all entry nodes are PAGE_SIZE, the data will be pre-aligned and
 * padded.
 */
int dm_bht_sync(struct dm_bht *bht, void *write_cb_ctx)
{
	unsigned int depth = 0;
	int ret = 0;
	int state;
	sector_t sector;
	struct dm_bht_level *level;
	struct dm_bht_entry *entry;
	struct dm_bht_entry *entry_end;
	
	BUG_ON(!bht);
	do {
		level = &bht->levels[depth];
		entry = level->entries;
		entry_end = level->entries + level->count;
		sector = level->sector;
		do {
			state = atomic_read(&entry->state);
			if (state <= DM_BHT_ENTRY_PENDING) {
				DMERR("At depth %d, entry %lu is not ready",
				      depth,
				      (unsigned long)(entry - level->entries));
				return state;
			}
			ret = bht->write_cb(write_cb_ctx,
					    sector,
					    entry->nodes,
					    to_sector(PAGE_SIZE),
					    entry);
			if (ret) {
				DMCRIT("an error occurred writing entry %lu",
				      (unsigned long)(entry - level->entries));
				return ret;
			}
			sector += to_sector(PAGE_SIZE);	
		} while (++entry < entry_end);
	} while (++depth < bht->depth);
	
	return 0;
}
EXPORT_SYMBOL_GPL(dm_bht_sync);

/**
 * dm_bht_populate - reads entries from disk needed to verify a given block
 * @bht:	pointer to a dm_bht_create()d bht
 * @read_cb_ctx:context used for all read_cb calls on this request
 * @block_index:specific block data is expected from
 *
 * Callers may wish to call dm_bht_populate(0) immediately after initialization
 * to start loading in all the level[0] entries.
 */
int dm_bht_populate(struct dm_bht *bht, void *read_cb_ctx, u32 block_index)
{
	unsigned int depth = 0;
	struct dm_bht_entry *entry;
	struct dm_bht_level *level;
	bool parent_loaded = false;
	int populated = 0;  /* return value */
	u32 entry_index = 0;
	u32 read_index = 0;
	u32 read_count = 0;
	int read_status = 0;
	unsigned int node_mask = 0;

	BUG_ON(!bht);
	if (block_index >= bht->block_count) {
		DMERR("Request to populate for invalid block: %u",
		      block_index);
		return -EIO;
	}
	DMDEBUG("dm_bht_populate(%u)", block_index);

	/* Load in all of level 0 if the root is unverified */
	if (!bht->root_verified) {
		DMDEBUG("root unverified. may need to be loaded");
		/* If positive, it means some are pending. */
		populated = dm_bht_maybe_read_entries(bht, read_cb_ctx, 0, 0,
						      bht->levels[0].count);
		if (populated < 0) {
			DMCRIT("an error occurred while reading level[0]");
			/* TODO(wad) define std error codes */
			return populated;
		}
	}

	/* Used to know if all the neighboring entries need to be loaded. */
	parent_loaded = true;
	node_mask = bht->node_count - 1;  /* pre-computed for easy masking */
	do {
		level = &bht->levels[depth];
		entry_index = block_index >>
			      ((bht->depth - depth) * bht->node_count_shift);
		/* XXX: BUG_ON(!level->entries) */
		entry = &level->entries[entry_index];

		/* parent_loaded avoids unecessary cmpxchgs over node_count
		 * entries at each level.  It, however, is not atomic so it
		 * is possible that the parent and all neighbors have been
		 * loaded in the intevening period. */
		if (!parent_loaded) {
			read_index = entry_index & (~(node_mask));
			read_count = min((u32)bht->node_count, 
					 level->count - read_index);
			DMDEBUG("!parent_loaded: %u (%u&~%u) %u",
				read_index, entry_index, node_mask, read_count);
		} else {
			read_index = entry_index;
			read_count = 1;
		}
		read_status = dm_bht_maybe_read_entries(bht, read_cb_ctx, depth,
							read_index, read_count);	
		if (unlikely(read_status < 0)) {
			DMCRIT("failure occurred reading entries: %u %u-%u",
			       depth, read_index, read_count);
			return read_status;
		}
		/* Accrue return code flags */
		populated |= read_status;
		/* Even if the parent has been loaded, we need to load for it if
		 * it hasn't been verified to ensure a verify call can succeed
		 */
		if (read_status)
			parent_loaded = false;
	} while (++depth < bht->depth);

	/* All nodes are ready. The hash for the block_index can be verified */
	return populated;
}
EXPORT_SYMBOL_GPL(dm_bht_populate);


/**
 * dm_bht_verify_block - checks that all nodes in the path for @block are valid
 * @bht:	pointer to a dm_bht_create()d bht
 * @block_index:specific block data is expected from
 * @digest:	computed digest for the given block to be checked
 * @digest_len:	length of @digest
 *
 * Returns 0 on success, 1 on missing data, and a negative error
 * code on verification failure. All supporting functions called
 * should return similarly.
 */
int dm_bht_verify_block(struct dm_bht *bht, u32 block_index, u8 *digest,
			unsigned int digest_len)
{
	int verified = 0;
	BUG_ON(!bht);
	
	/* TODO(wad) do we really need this? */
	if (digest_len != bht->digest_size) {
		DMERR("invalid digest_len passed to verify_block");
		return -EINVAL;
	}

	/* Make sure that the root has been verified */
	if (!bht->root_verified) {
		verified = dm_bht_verify_root(bht, dm_bht_compare_hash);
		if (verified) {
			DMCRIT("Failed to verify root: %d", verified);
			return verified;
		}
	}

	/* Now check that the digest supplied matches the leaf hash */
	verified = dm_bht_check_block(bht, block_index, digest);
	if (verified) {
		DMCRIT("Block check failed for %u: %d", block_index, verified);
		return verified;
	}

	/* Now check levels in between */
	verified = dm_bht_verify_levels(bht,
					block_index,
					dm_bht_compare_hash);
	if (verified) {
		DMERR("Failed to verify intermediary nodes for block: %u",
		      block_index);
	}
	return verified;
}
EXPORT_SYMBOL_GPL(dm_bht_verify_block);

/**
 * dm_bht_destroy - cleans up all memory used by @bht
 * @bht:	pointer to a dm_bht_create()d bht
 *
 * Returns 0 on success. Does not free @bht itself.
 */
int dm_bht_destroy(struct dm_bht *bht)
{
	unsigned int depth;
	int cpu = 0;
	BUG_ON(!bht);	

	if (bht->root_digest)
		kfree(bht->root_digest);

	depth = bht->depth;
	while (depth-- != 0) {
		struct dm_bht_entry *entry = bht->levels[depth].entries;
		struct dm_bht_entry *entry_end = entry +
						 bht->levels[depth].count;
		int state = 0;
		do {
			state = atomic_read(&entry->state);
			switch (state) {
			/* At present, no other states free memory,
			 * but that will change. */
			case DM_BHT_ENTRY_UNALLOCATED:
				/* Allocated with improper state */
				BUG_ON(entry->nodes);
				continue;
			default:
				BUG_ON(!entry->nodes);
				mempool_free(entry->nodes, bht->entry_pool);
				break;
			}
		} while (++entry < entry_end);
		kfree(bht->levels[depth].entries);
		bht->levels[depth].entries = NULL;
	}
	mempool_destroy(bht->entry_pool);
	kfree(bht->levels);
	for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
		if (bht->hash_desc[cpu].tfm)
			crypto_free_hash(bht->hash_desc[cpu].tfm);
	kfree(bht->hash_desc);
	return 0;
}
EXPORT_SYMBOL_GPL(dm_bht_destroy);

/*-----------------------------------------------
 * Accessors
 *-----------------------------------------------*/

/**
 * dm_bht_sectors - return the sectors required on disk
 * @bht:	pointer to a dm_bht_create()d bht
 */
sector_t dm_bht_sectors(const struct dm_bht *bht)
{
	BUG_ON(!bht);
	return bht->sectors;
}
EXPORT_SYMBOL_GPL(dm_bht_sectors);

/**
 * dm_bht_set_read_cb - set read callback
 * @bht:	pointer to a dm_bht_create()d bht
 * @read_cb:	callback function used for all read requests by @bht
 */
void dm_bht_set_read_cb(struct dm_bht *bht, dm_bht_callback read_cb)
{
	BUG_ON(!bht);
	bht->read_cb = read_cb;
}
EXPORT_SYMBOL_GPL(dm_bht_set_read_cb);

/**
 * dm_bht_set_write_cb - set write callback
 * @bht:	pointer to a dm_bht_create()d bht
 * @write_cb:	callback function used for all write requests by @bht
 */
void dm_bht_set_write_cb(struct dm_bht *bht, dm_bht_callback write_cb)
{
	BUG_ON(!bht);
	bht->write_cb = write_cb;
}
EXPORT_SYMBOL_GPL(dm_bht_set_write_cb);

/**
 * dm_bht_set_root_hexdigest - sets an unverified root digest hash from hex
 * @bht:	pointer to a dm_bht_create()d bht
 * @hexdigest:	array of u8s containing the new digest in binary
 * Returns non-zero on error.  hexdigest should be NUL terminated.
 */
int dm_bht_set_root_hexdigest(struct dm_bht *bht, const u8 *hexdigest)
{
	BUG_ON(!bht || !hexdigest);
	if (!bht->root_digest) {
		DMCRIT("No allocation for root digest. Call dm_bht_create");
		return -1;
	}
	/* Make sure we have at least the bytes expected */
	if (strnlen((char *)hexdigest, bht->digest_size * 2) !=
	    bht->digest_size * 2) {
		DMERR("root digest length does not match hash algorithm");
		return -1;
	}
	dm_bht_hex_to_bin(bht->root_digest, hexdigest, bht->digest_size);
#ifdef CONFIG_DM_DEBUG
	DMINFO("Set root digest to %s. Parsed as -> ", hexdigest);
	dm_bht_log_mismatch(bht, bht->root_digest, bht->root_digest);
#endif
	bht->root_verified = false;
	return 0;
}
EXPORT_SYMBOL_GPL(dm_bht_set_root_hexdigest);

/**
 * dm_bht_root_hexdigest - returns root digest in hex
 * @bht:	pointer to a dm_bht_create()d bht
 * @hexdigest:	u8 array of size @available
 * @available:	must be bht->digest_size * 2 + 1
 */
int dm_bht_root_hexdigest(struct dm_bht *bht, u8 *hexdigest, int available)
{
	BUG_ON(!bht);
	if (available < 0 ||
	    ((unsigned int) available) < bht->digest_size * 2 + 1) {
		DMERR("hexdigest has too few bytes available");
		return -EINVAL;
	}
	if (!bht->root_digest) {
		DMERR("no root digest exists to export");
		if (available > 0) {
			*hexdigest = 0;
		}
		return -1;
	}
	dm_bht_bin_to_hex(bht->root_digest, hexdigest, bht->digest_size);
	hexdigest[bht->digest_size * 2] = 0;
	return 0;
}
EXPORT_SYMBOL_GPL(dm_bht_root_hexdigest);

