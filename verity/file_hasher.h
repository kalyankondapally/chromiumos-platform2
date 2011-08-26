// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by the GPL v2 license that can
// be found in the LICENSE file.
//
// Defines FileHasher, a class that creates a Verity-specific file of
// per-block hashes from a given simple_file::File.
#ifndef VERITY_FILE_HASHER_H__
#define VERITY_FILE_HASHER_H__ 1

extern "C" {
#include "verity/dm-bht.h"
}
#include "verity/simple_file/file.h"
#include "verity/include/asm/page.h"

namespace verity {
// FileHasher takes a simple_file::File object and reads in |block_size|
// bytes creating SHA-256 hashes as it goes.
// TODO(wad) allow any hashing format supported by openssl (and the kernel).
// This class may not be used by multiple threads at once.
class FileHasher {
 public:
  FileHasher() : source_(NULL),
                 destination_(NULL),
                 block_limit_(0),
                 alg_(NULL) { }
  // TODO(wad) add initialized_ variable to check.
  virtual ~FileHasher() { dm_bht_destroy(&tree_); }
  virtual bool Initialize(simple_file::File *source,
                          simple_file::File *destination,
                          unsigned int blocks,
                          const char *alg);
  virtual bool Hash();
  virtual bool Store();
  // Print a table to stdout which contains a dmsetup compatible format
  virtual void PrintTable(bool colocated);

  virtual void set_salt(const char *salt) {
    dm_bht_set_salt(&tree_, salt);
    salt_ = salt;
  }
  virtual const char *salt(void) { return salt_; }

  static int WriteCallback(void *file,
                           sector_t start,
                           u8 *dst,
                           sector_t count,
                           struct dm_bht_entry *entry);
 private:
  simple_file::File *source_;
  simple_file::File *destination_;
  unsigned int block_limit_;
  const char *alg_;
  const char *salt_;
  struct dm_bht tree_;
};

}  // namespace verity

#endif   // VERITY_FILE_HASHER_H__
