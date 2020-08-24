/*
 * Copyright (C) 2010 The Chromium OS Authors <chromium-os-dev@chromium.org>
 *
 * Device-Mapper block hash tree interface.
 * See Documentation/device-mapper/dm-bht.txt for details.
 *
 * This file is released under the GPL.
 */

#include <limits.h>
#include <string.h>

#include <asm/page.h>
#include <linux/bitops.h> /* for fls() */
#include <linux/bug.h>
/* #define CONFIG_DM_DEBUG 1 */
#include <linux/device-mapper.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#include "verity/dm-bht.h"

#define DM_MSG_PREFIX "dm bht"

/* For sector formatting. */
#if defined(_LP64) || defined(__LP64__) || __BITS_PER_LONG == 64
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX "ll"
#endif
#define PRIu64 __PRIS_PREFIX "u"

/*-----------------------------------------------
 * Utilities
 *-----------------------------------------------*/

/* We assume we only have one CPU in userland. */
#define nr_cpu_ids 1
#define smp_processor_id(_x) 0

static inline void* alloc_page(void) {
  void* memptr;

  if (posix_memalign((void**)&memptr, PAGE_SIZE, PAGE_SIZE))
    return NULL;
  return memptr;
}

static u8 from_hex(u8 ch) {
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
 * @binary: a byte array of length @binary_len
 * @hex: a byte array of length @binary_len * 2 + 1
 */
static void dm_bht_bin_to_hex(u8* binary, u8* hex, unsigned int binary_len) {
  while (binary_len-- > 0) {
    sprintf((char* __restrict__)hex, "%02hhx", (unsigned char)*binary);
    hex += 2;
    binary++;
  }
}

/**
 * dm_bht_hex_to_bin - converts a hex stream to binary
 * @binary: a byte array of length @binary_len
 * @hex: a byte array of length @binary_len * 2 + 1
 */
static void dm_bht_hex_to_bin(u8* binary,
                              const u8* hex,
                              unsigned int binary_len) {
  while (binary_len-- > 0) {
    *binary = from_hex(*(hex++));
    *binary *= 16;
    *binary += from_hex(*(hex++));
    binary++;
  }
}

static void dm_bht_log_mismatch(struct dm_bht* bht, u8* given, u8* computed) {
  u8 given_hex[DM_BHT_MAX_DIGEST_SIZE * 2 + 1];
  u8 computed_hex[DM_BHT_MAX_DIGEST_SIZE * 2 + 1];
  dm_bht_bin_to_hex(given, given_hex, bht->digest_size);
  dm_bht_bin_to_hex(computed, computed_hex, bht->digest_size);
  DMERR_LIMIT("%s != %s", given_hex, computed_hex);
}

/* Used for turning verifiers into computers */
typedef int (*dm_bht_compare_cb)(struct dm_bht*, u8*, u8*);

/**
 * dm_bht_compute_hash: hashes a page of data
 */
static int dm_bht_compute_hash(struct dm_bht* bht,
                               const u8* buffer,
                               u8* digest) {
  struct hash_desc* hash_desc = &bht->hash_desc[smp_processor_id()];

  /* Note, this is synchronous. */
  if (crypto_hash_init(hash_desc)) {
    DMCRIT("failed to reinitialize crypto hash (proc:%d)", smp_processor_id());
    return -EINVAL;
  }
  if (crypto_hash_update(hash_desc, buffer, PAGE_SIZE)) {
    DMCRIT("crypto_hash_update failed");
    return -EINVAL;
  }
  if (bht->have_salt) {
    if (crypto_hash_update(hash_desc, bht->salt, sizeof(bht->salt))) {
      DMCRIT("crypto_hash_update failed");
      return -EINVAL;
    }
  }
  if (crypto_hash_final(hash_desc, digest)) {
    DMCRIT("crypto_hash_final failed");
    return -EINVAL;
  }

  return 0;
}

/*-----------------------------------------------
 * Implementation functions
 *-----------------------------------------------*/

static int dm_bht_initialize_entries(struct dm_bht* bht);

static int dm_bht_read_callback_stub(void* ctx,
                                     sector_t start,
                                     u8* dst,
                                     sector_t count,
                                     struct dm_bht_entry* entry);

/**
 * dm_bht_create - prepares @bht for us
 * @bht: pointer to a dm_bht_create()d bht
 * @depth: tree depth without the root; including block hashes
 * @block_count:the number of block hashes / tree leaves
 * @alg_name: crypto hash algorithm name
 *
 * Returns 0 on success.
 *
 * Callers can offset into devices by storing the data in the io callbacks.
 * TODO(wad) bust up into smaller helpers
 */
int dm_bht_create(struct dm_bht* bht,
                  unsigned int block_count,
                  const char* alg_name) {
  int status = 0;
  int cpu = 0;

  bht->have_salt = false;

  /* Setup the hash first. Its length determines much of the bht layout */
  for (cpu = 0; cpu < nr_cpu_ids; ++cpu) {
    bht->hash_desc[cpu].tfm = crypto_alloc_hash(alg_name, 0, 0);
    if (bht->hash_desc[cpu].tfm == NULL) {
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

  /* Configure the tree */
  bht->block_count = block_count;
  DMDEBUG("Setting block_count %u", block_count);
  if (block_count == 0) {
    DMERR("block_count must be non-zero");
    status = -EINVAL;
    goto bad_block_count;
  }

  /* Each dm_bht_entry->nodes is one page.  The node code tracks
   * how many nodes fit into one entry where a node is a single
   * hash (message digest).
   */
  bht->node_count_shift = fls(PAGE_SIZE / bht->digest_size) - 1;
  /* Round down to the nearest power of two.  This makes indexing
   * into the tree much less painful.
   */
  bht->node_count = 1 << bht->node_count_shift;

  /* This is unlikely to happen, but with 64k pages, who knows. */
  if (bht->node_count > UINT_MAX / bht->digest_size) {
    DMERR("node_count * hash_len exceeds UINT_MAX!");
    status = -EINVAL;
    goto bad_node_count;
  }

  bht->depth = DIV_ROUND_UP(fls(block_count - 1), bht->node_count_shift);
  DMDEBUG("Setting depth to %d.", bht->depth);

  /* Ensure that we can safely shift by this value. */
  if (bht->depth * bht->node_count_shift >= sizeof(unsigned int) * 8) {
    DMERR("specified depth and node_count_shift is too large");
    status = -EINVAL;
    goto bad_node_count;
  }

  /* Allocate levels. Each level of the tree may have an arbitrary number
   * of dm_bht_entry structs.  Each entry contains node_count nodes.
   * Each node in the tree is a cryptographic digest of either node_count
   * nodes on the subsequent level or of a specific block on disk.
   */
  bht->levels =
      (struct dm_bht_level*)calloc(bht->depth, sizeof(struct dm_bht_level));
  if (!bht->levels) {
    DMERR("failed to allocate tree levels");
    status = -ENOMEM;
    goto bad_level_alloc;
  }

  /* Setup read callback stub */
  bht->read_cb = &dm_bht_read_callback_stub;

  status = dm_bht_initialize_entries(bht);
  if (status)
    goto bad_entries_alloc;

  /* We compute depth such that there is only be 1 block at level 0. */
  BUG_ON(bht->levels[0].count != 1);

  return 0;

bad_entries_alloc:
  while (bht->depth-- > 0)
    free(bht->levels[bht->depth].entries);
  free(bht->levels);
bad_node_count:
bad_level_alloc:
bad_block_count:
bad_digest_len:
bad_hash_alg:
  for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
    if (bht->hash_desc[cpu].tfm)
      crypto_free_hash(bht->hash_desc[cpu].tfm);
  return status;
}

static int dm_bht_initialize_entries(struct dm_bht* bht) {
  /* The last_index represents the index into the last
   * block digest that will be stored in the tree.  By walking the
   * tree with that index, it is possible to compute the total number
   * of entries needed at each level in the tree.
   *
   * Since each entry will contain up to |node_count| nodes of the tree,
   * it is possible that the last index may not be at the end of a given
   * entry->nodes.  In that case, it is assumed the value is padded.
   *
   * Note, we treat both the tree root (1 hash) and the tree leaves
   * independently from the bht data structures.  Logically, the root is
   * depth=-1 and the block layer level is depth=bht->depth
   */
  unsigned int last_index = ALIGN(bht->block_count, bht->node_count) - 1;
  unsigned int total_entries = 0;
  struct dm_bht_level* level = NULL;
  int depth;

  /* check that the largest level->count can't result in an int overflow
   * on allocation or sector calculation.
   */
  if (((last_index >> bht->node_count_shift) + 1) >
      UINT_MAX / MAX((unsigned int)sizeof(struct dm_bht_entry),
                     (unsigned int)to_sector(PAGE_SIZE))) {
    DMCRIT("required entries %u is too large", last_index + 1);
    return -EINVAL;
  }

  /* Track the current sector location for each level so we don't have to
   * compute it during traversals.
   */
  bht->sectors = 0;
  for (depth = 0; depth < bht->depth; ++depth) {
    level = dm_bht_get_level(bht, depth);
    level->count = dm_bht_index_at_level(bht, depth, last_index) + 1;
    DMDEBUG("depth: %d entries: %u", depth, level->count);
    /* TODO(wad) consider the case where the data stored for each
     * level is done with contiguous pages (instead of using
     * entry->nodes) and the level just contains two bitmaps:
     * (a) which pages have been loaded from disk
     * (b) which specific nodes have been verified.
     */
    level->entries =
        (struct dm_bht_entry*)calloc(level->count, sizeof(struct dm_bht_entry));
    if (!level->entries) {
      DMERR("failed to allocate entries for depth %d", bht->depth);
      /* let the caller clean up the mess */
      return -ENOMEM;
    }
    total_entries += level->count;
    level->sector = bht->sectors;
    /* number of sectors per entry * entries at this level */
    bht->sectors += level->count * to_sector(PAGE_SIZE);
    /* not ideal, but since unsigned overflow behavior is defined */
    if (bht->sectors < level->sector) {
      DMCRIT("level sector calculation overflowed");
      return -EINVAL;
    }
  }

  return 0;
}

static int dm_bht_read_callback_stub(void* ctx,
                                     sector_t start,
                                     u8* dst,
                                     sector_t count,
                                     struct dm_bht_entry* entry) {
  DMCRIT("dm_bht_read_callback_stub called!");
  dm_bht_read_completed(entry, -EIO);
  return -EIO;
}

/**
 * dm_bht_read_completed
 * @entry: pointer to the entry that's been loaded
 * @status: I/O status. Non-zero is failure.
 * MUST always be called after a read_cb completes.
 */
void dm_bht_read_completed(struct dm_bht_entry* entry, int status) {
  if (status) {
    /* TODO(wad) add retry support */
    DMCRIT("an I/O error occurred while reading entry");
    entry->state = DM_BHT_ENTRY_ERROR_IO;
    /* entry->nodes will be freed later */
    return;
  }
  BUG_ON(entry->state != DM_BHT_ENTRY_PENDING);
  entry->state = DM_BHT_ENTRY_READY;
}

/* dm_bht_verify_path
 * Verifies the path. Returns 0 on ok.
 */
static int dm_bht_verify_path(struct dm_bht* bht,
                              unsigned int block,
                              const u8* buffer) {
  int depth = bht->depth;
  u8 digest[DM_BHT_MAX_DIGEST_SIZE];
  struct dm_bht_entry* entry;
  u8* node;
  int state;

  do {
    /* Need to check that the hash of the current block is accurate
     * in its parent.
     */
    entry = dm_bht_get_entry(bht, depth - 1, block);
    state = entry->state;
    /* This call is only safe if all nodes along the path
     * are already populated (i.e. READY) via dm_bht_populate.
     */
    BUG_ON(state < DM_BHT_ENTRY_READY);
    node = dm_bht_get_node(bht, entry, depth, block);

    if (dm_bht_compute_hash(bht, buffer, digest) ||
        memcmp(digest, node, bht->digest_size))
      goto mismatch;

    /* Keep the containing block of hashes to be verified in the
     * next pass.
     */
    buffer = entry->nodes;
  } while (--depth > 0 && state != DM_BHT_ENTRY_VERIFIED);

  if (depth == 0 && state != DM_BHT_ENTRY_VERIFIED) {
    if (dm_bht_compute_hash(bht, buffer, digest) ||
        memcmp(digest, bht->root_digest, bht->digest_size))
      goto mismatch;
    entry->state = DM_BHT_ENTRY_VERIFIED;
  }

  /* Mark path to leaf as verified. */
  for (depth++; depth < bht->depth; depth++) {
    entry = dm_bht_get_entry(bht, depth, block);
    /* At this point, entry can only be in VERIFIED or READY state.
     */
    entry->state = DM_BHT_ENTRY_VERIFIED;
  }

  DMDEBUG("verify_path: node %u is verified to root", block);
  return 0;

mismatch:
  DMERR_LIMIT("verify_path: failed to verify hash (d=%d,bi=%u)", depth, block);
  dm_bht_log_mismatch(bht, node, digest);
  return DM_BHT_ENTRY_ERROR_MISMATCH;
}

/**
 * dm_bht_zeroread_callback - read callback which always returns 0s
 * @ctx: ignored
 * @start: ignored
 * @data: buffer to write 0s to
 * @count: number of sectors worth of data to write
 * @complete_ctx: opaque context for @completed
 * @completed: callback to confirm end of data read
 *
 * Always returns 0.
 *
 * Meant for use by dm_compute() callers.  It allows dm_populate to
 * be used to pre-fill a tree with zeroed out entry nodes.
 */
int dm_bht_zeroread_callback(void* ctx,
                             sector_t start,
                             u8* dst,
                             sector_t count,
                             struct dm_bht_entry* entry) {
  memset(dst, 0, verity_to_bytes(count));
  dm_bht_read_completed(entry, 0);
  return 0;
}

/**
 * dm_bht_is_populated - check that entries from disk needed to verify a given
 *                       block are all ready
 * @bht: pointer to a dm_bht_create()d bht
 * @block: specific block data is expected from
 *
 * Callers may wish to call dm_bht_is_populated() when checking an io
 * for which entries were already pending.
 */
bool dm_bht_is_populated(struct dm_bht* bht, unsigned int block) {
  int depth;

  for (depth = bht->depth - 1; depth >= 0; depth--) {
    struct dm_bht_entry* entry = dm_bht_get_entry(bht, depth, block);
    if (entry->state < DM_BHT_ENTRY_READY)
      return false;
  }

  return true;
}

/**
 * dm_bht_populate - reads entries from disk needed to verify a given block
 * @bht: pointer to a dm_bht_create()d bht
 * @ctx: context used for all read_cb calls on this request
 * @block: specific block data is expected from
 *
 * Returns negative value on error. Returns 0 on success.
 */
int dm_bht_populate(struct dm_bht* bht, void* ctx, unsigned int block) {
  int depth;
  int state = 0;

  BUG_ON(block >= bht->block_count);

  DMDEBUG("dm_bht_populate(%u)", block);

  for (depth = bht->depth - 1; depth >= 0; --depth) {
    struct dm_bht_level* level;
    struct dm_bht_entry* entry;
    unsigned int index;
    u8* buffer;

    entry = dm_bht_get_entry(bht, depth, block);
    state = entry->state;
    if (state == DM_BHT_ENTRY_UNALLOCATED)
      entry->state = DM_BHT_ENTRY_PENDING;

    if (state == DM_BHT_ENTRY_VERIFIED)
      break;
    if (state <= DM_BHT_ENTRY_ERROR)
      goto error_state;
    if (state != DM_BHT_ENTRY_UNALLOCATED)
      continue;

    /* Current entry is claimed for allocation and loading */
    buffer = (u8*)alloc_page();
    if (!buffer)
      goto nomem;

    /* dm-bht guarantees page-aligned memory for callbacks. */
    entry->nodes = buffer;

    /* TODO(wad) error check callback here too */

    level = &bht->levels[depth];
    index = dm_bht_index_at_level(bht, depth, block);
    bht->read_cb(ctx, level->sector + to_sector(index * PAGE_SIZE),
                 entry->nodes, to_sector(PAGE_SIZE), entry);
  }

  return 0;

error_state:
  DMCRIT("block %u at depth %d is in an error state", block, depth);
  return state;

nomem:
  DMCRIT("failed to allocate memory for entry->nodes");
  return -ENOMEM;
}

/**
 * dm_bht_verify_block - checks that all nodes in the path for @block are valid
 * @bht: pointer to a dm_bht_create()d bht
 * @block: specific block data is expected from
 * @buffer: page holding the block data
 * @offset: offset into the page
 *
 * Returns 0 on success, 1 on missing data, and a negative error
 * code on verification failure. All supporting functions called
 * should return similarly.
 */
int dm_bht_verify_block(struct dm_bht* bht,
                        unsigned int block,
                        const u8* buffer,
                        unsigned int offset) {
  BUG_ON(offset != 0);

  return dm_bht_verify_path(bht, block, buffer);
}

/**
 * dm_bht_destroy - cleans up all memory used by @bht
 * @bht: pointer to a dm_bht_create()d bht
 *
 * Returns 0 on success. Does not free @bht itself.
 */
int dm_bht_destroy(struct dm_bht* bht) {
  int depth;
  int cpu = 0;

  depth = bht->depth;
  while (depth-- != 0) {
    struct dm_bht_entry* entry = bht->levels[depth].entries;
    struct dm_bht_entry* entry_end = entry + bht->levels[depth].count;
    for (; entry < entry_end; ++entry) {
      switch (entry->state) {
        /* At present, no other states free memory,
         * but that will change.
         */
        case DM_BHT_ENTRY_UNALLOCATED:
          /* Allocated with improper state */
          BUG_ON(entry->nodes);
          continue;
        default:
          BUG_ON(!entry->nodes);
          free(entry->nodes);
          break;
      }
    }
    free(bht->levels[depth].entries);
    bht->levels[depth].entries = NULL;
  }
  free(bht->levels);
  for (cpu = 0; cpu < nr_cpu_ids; ++cpu)
    if (bht->hash_desc[cpu].tfm)
      crypto_free_hash(bht->hash_desc[cpu].tfm);
  return 0;
}

/*-----------------------------------------------
 * Accessors
 *-----------------------------------------------*/

/**
 * dm_bht_sectors - return the sectors required on disk
 * @bht: pointer to a dm_bht_create()d bht
 */
sector_t dm_bht_sectors(const struct dm_bht* bht) {
  return bht->sectors;
}

/**
 * dm_bht_set_read_cb - set read callback
 * @bht: pointer to a dm_bht_create()d bht
 * @read_cb: callback function used for all read requests by @bht
 */
void dm_bht_set_read_cb(struct dm_bht* bht, dm_bht_callback read_cb) {
  bht->read_cb = read_cb;
}

/**
 * dm_bht_set_root_hexdigest - sets an unverified root digest hash from hex
 * @bht: pointer to a dm_bht_create()d bht
 * @hexdigest: array of u8s containing the new digest in binary
 * Returns non-zero on error.  hexdigest should be NUL terminated.
 */
int dm_bht_set_root_hexdigest(struct dm_bht* bht, const u8* hexdigest) {
  /* Make sure we have at least the bytes expected */
  if (strnlen((char*)hexdigest, bht->digest_size * 2) != bht->digest_size * 2) {
    DMERR("root digest length does not match hash algorithm");
    return -1;
  }
  dm_bht_hex_to_bin(bht->root_digest, hexdigest, bht->digest_size);
#ifdef CONFIG_DM_DEBUG
  DMINFO("Set root digest to %s. Parsed as -> ", hexdigest);
  dm_bht_log_mismatch(bht, bht->root_digest, bht->root_digest);
#endif
  return 0;
}

/**
 * dm_bht_root_hexdigest - returns root digest in hex
 * @bht: pointer to a dm_bht_create()d bht
 * @hexdigest: u8 array of size @available
 * @available: must be bht->digest_size * 2 + 1
 */
int dm_bht_root_hexdigest(struct dm_bht* bht, u8* hexdigest, int available) {
  if (available < 0 || ((unsigned int)available) < bht->digest_size * 2 + 1) {
    DMERR("hexdigest has too few bytes available");
    return -EINVAL;
  }
  dm_bht_bin_to_hex(bht->root_digest, hexdigest, bht->digest_size);
  return 0;
}

/**
 * dm_bht_set_salt - sets the salt used, in hex
 * @bht: pointer to a dm_bht_create()d bht
 * @hexsalt: salt string, as hex; will be zero-padded or truncated to
 *            DM_BHT_SALT_SIZE * 2 hex digits.
 */
void dm_bht_set_salt(struct dm_bht* bht, const char* hexsalt) {
  size_t saltlen = MIN(strlen(hexsalt) / 2, sizeof(bht->salt));
  bht->have_salt = true;
  memset(bht->salt, 0, sizeof(bht->salt));
  dm_bht_hex_to_bin(bht->salt, (const u8*)hexsalt, saltlen);
}

/**
 * dm_bht_salt - returns the salt used, in hex
 * @bht: pointer to a dm_bht_create()d bht
 * @hexsalt: buffer to put salt into, of length DM_BHT_SALT_SIZE * 2 + 1.
 */
int dm_bht_salt(struct dm_bht* bht, char* hexsalt) {
  if (!bht->have_salt)
    return -EINVAL;
  dm_bht_bin_to_hex(bht->salt, (u8*)hexsalt, sizeof(bht->salt));
  return 0;
}