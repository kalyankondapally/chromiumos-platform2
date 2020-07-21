// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OCR_DAEMON_H_
#define OCR_DAEMON_H_

#include <memory>

#include <base/memory/weak_ptr.h>
#include <brillo/daemons/dbus_daemon.h>
#include <mojo/core/embedder/scoped_ipc_support.h>

namespace ocr {

// Optical Character Recognition daemon with D-Bus support.
// The primary function of the D-Bus interface is to receive Mojo
// bootstrap requests from clients.
class OcrDaemon : public brillo::DBusDaemon {
 public:
  OcrDaemon();
  ~OcrDaemon() override;
  OcrDaemon(const OcrDaemon&) = delete;
  OcrDaemon& operator=(const OcrDaemon&) = delete;

 protected:
  // brillo:DBusDaemon:
  int OnInit() override;

 private:
  // As long as this object is alive, all Mojo API surfaces relevant to IPC
  // connections are usable and message pipes which span a process boundary
  // will continue to function.
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  // Member variables should appear before the WeakPtrFactory to ensure
  // that any WeakPtrs to OcrDaemon are invalidated before its member
  // variables' destructors are executed, rendering them invalid.
  // Members are destructed in reverse-order that they appear in the
  // class definition, so this must be the last class member.
  base::WeakPtrFactory<OcrDaemon> weak_ptr_factory_{this};
};

}  // namespace ocr

#endif  // OCR_DAEMON_H_