// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/message_stream.h"

#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>

namespace arc {

MessageStream::MessageStream(base::ScopedFD fd) : fd_(std::move(fd)) {}

MessageStream::~MessageStream() = default;

bool MessageStream::Read(arc_proxy::VSockMessage* message) {
  uint64_t size = 0;
  if (!base::ReadFromFD(fd_.get(), reinterpret_cast<char*>(&size),
                        sizeof(size))) {
    PLOG(ERROR) << "Failed to read message size";
    return false;
  }

  buf_.resize(size);
  if (!base::ReadFromFD(fd_.get(), buf_.data(), buf_.size())) {
    PLOG(ERROR) << "Failed to read a proto";
    return false;
  }

  if (!message->ParseFromArray(buf_.data(), buf_.size())) {
    LOG(ERROR) << "Failed to parse proto message";
    return false;
  }
  return true;
}

bool MessageStream::Write(const arc_proxy::VSockMessage& message) {
  const uint64_t size = message.ByteSize();
  buf_.resize(sizeof(size) + size);

  struct Frame {
    uint64_t size;
    char data[];
  };
  Frame* frame = reinterpret_cast<Frame*>(buf_.data());
  frame->size = size;
  if (!message.SerializeToArray(frame->data, size)) {
    LOG(ERROR) << "Failed to serialize proto.";
    return false;
  }

  if (!base::WriteFileDescriptor(fd_.get(), buf_.data(), buf_.size())) {
    PLOG(ERROR) << "Failed to write proto";
    return false;
  }
  return true;
}

}  // namespace arc