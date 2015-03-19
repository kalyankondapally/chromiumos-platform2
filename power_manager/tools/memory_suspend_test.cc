// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <chromeos/flag_helper.h>

#define PATTERN(i) ((i % 1) ? 0x55555555 : 0xAAAAAAAA)
#define SIZE_2_9_GB (2900LL * 1024LL * 1024LL)

using base::FilePath;
using std::set;
using std::string;
using std::vector;

void PrintAddrMap(void *vaddr) {
  int fd;
  uintptr_t page = reinterpret_cast<uintptr_t>(vaddr) / getpagesize();
  uint64_t page_data;

  fd = open("/proc/self/pagemap", O_RDONLY);
  CHECK_GE(fd, 0);
  CHECK_EQ(static_cast<uintptr_t>(lseek64(fd, page * 8, SEEK_SET)), page * 8);
  CHECK_EQ(read(fd, &page_data, 8), 8);
  printf("Vaddr: 0x%p   PFN=0x%llx  shift=%llu  present=%lld\n", vaddr,
         page_data & ((1LL << 55) - 1), (page_data & ((0x3fLL << 55))) >> 55,
         (page_data & (1LL << 63)) >> 63);
}

int Suspend(uint64_t wakeup_count) {
  return system(base::StringPrintf(
      "powerd_dbus_suspend --delay=0 --wakeup_count=%" PRIu64,
      wakeup_count).c_str());
}

uint32_t* Allocate(size_t size) {
  return static_cast<uint32_t *>(malloc(size));
}

void Fill(uint32_t *ptr, size_t size) {
  for (size_t i = 0; i < size / sizeof(*ptr); i++) {
    *(ptr + i) = PATTERN(i);
  }
}

bool Check(uint32_t *ptr, size_t size) {
  bool success = true;

  for (size_t i = 0; i < size / sizeof(*ptr); i++) {
    if (*(ptr + i) != PATTERN(i)) {
      printf("Found changed value: Addr=%p val=0x%X, expected=0x%X\n",
             ptr + i, *(ptr + i), PATTERN(i));
      PrintAddrMap(ptr + i);
      success = false;
    }
  }
  return success;
}

int64_t GetUsableMemorySize() {
  int64_t size = 0;

  /* Read /proc/meminfo */
  string meminfo_raw;
  const FilePath meminfo_path("/proc/meminfo");
  CHECK(base::ReadFileToString(meminfo_path, &meminfo_raw));

  /* Parse /proc/meminfo for MemFree and Inactive size */
  set<string> field_name = {"MemFree", "Inactive"};
  vector<string> lines;

  Tokenize(meminfo_raw, "\n", &lines);
  for (auto line : lines) {
    vector<string> tokens;
    Tokenize(line, ": ", &tokens);
    auto it = field_name.find(tokens[0]);
    if (it != field_name.end()) {
      uint64_t field_value;
      CHECK(base::StringToUint64(tokens[1], &field_value));
      size += field_value;
      field_name.erase(it);
    }
  }
  CHECK(field_name.empty());

  /* Size should be Free + Inactive - 192 MiB */
  size -= 192 * 1024;

  /* Size is in KB right now */
  size *= 1024;

  CHECK_GT(size, 0);
  return size;
}

int main(int argc, char* argv[]) {
  DEFINE_int64(size, 0, "Amount of memory to allocate");
  DEFINE_uint64(wakeup_count, 0, "Value read from /sys/power/wakeup_count");

  chromeos::FlagHelper::Init(argc, argv,
      "Test memory retention across suspend/resume.\n\n"
      "  Fills memory with 0x55/0xAA patterns, performs a suspend, and checks\n"
      "  those patterns after resume. Will return 0 on success, 1 when the\n"
      "  suspend operation fails, and 2 when memory errors were detected.");

  int64_t size = FLAGS_size;
  bool autosize = false;
  if (size == 0) {
    autosize = true;
    size = GetUsableMemorySize();
  }

  uint32_t *ptr = Allocate(size);

  /* Retry allocate at 2.9GiB on 32 bit userland machine */
  /* NOLINTNEXTLINE(runtime/int) - suppress using long instead of int32 */
  if (!ptr && autosize && sizeof(long) == 4 && size > SIZE_2_9_GB) {
    size = SIZE_2_9_GB;
    ptr = Allocate(size);
  }

  CHECK(ptr);

  Fill(ptr, size);
  if (Suspend(FLAGS_wakeup_count)) {
    printf("Error suspending\n");
    return 1;
  }
  if (Check(ptr, size))
    return 0;
  // The power_MemorySuspend Autotest depends on this value.
  return 2;
}
