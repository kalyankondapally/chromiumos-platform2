// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/smbfs_bootstrap_impl.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <mojo/public/cpp/system/platform_handle.h>

#include "smbfs/smb_credential.h"
#include "smbfs/smb_filesystem.h"
#include "smbfs/smbfs_impl.h"

namespace smbfs {
namespace {

mojom::MountError ConnectErrorToMountError(SmbFilesystem::ConnectError error) {
  switch (error) {
    case SmbFilesystem::ConnectError::kNotFound:
      return mojom::MountError::kNotFound;
    case SmbFilesystem::ConnectError::kAccessDenied:
      return mojom::MountError::kAccessDenied;
    case SmbFilesystem::ConnectError::kSmb1Unsupported:
      return mojom::MountError::kInvalidProtocol;
    default:
      return mojom::MountError::kUnknown;
  }
}

std::unique_ptr<password_provider::Password> MakePasswordFromMojoHandle(
    mojo::ScopedHandle handle, int32_t length) {
  base::ScopedFD fd = mojo::UnwrapPlatformHandle(std::move(handle)).TakeFD();
  return password_provider::Password::CreateFromFileDescriptor(fd.get(),
                                                               length);
}

}  // namespace

SmbFsBootstrapImpl::SmbFsBootstrapImpl(mojom::SmbFsBootstrapRequest request,
                                       Delegate* delegate)
    : binding_(this, std::move(request)), delegate_(delegate) {
  DCHECK(delegate_);
  binding_.set_connection_error_handler(base::Bind(
      &Delegate::OnBootstrapConnectionError, base::Unretained(delegate)));
}

SmbFsBootstrapImpl::~SmbFsBootstrapImpl() = default;

void SmbFsBootstrapImpl::MountShare(mojom::MountOptionsPtr options,
                                    mojom::SmbFsDelegatePtr smbfs_delegate,
                                    const MountShareCallback& callback) {
  if (options->share_path.find("smb://") != 0) {
    // TODO(amistry): More extensive URL validation.
    LOG(ERROR) << "Invalid share path: " << options->share_path;
    callback.Run(mojom::MountError::kInvalidUrl, nullptr);
    return;
  }

  std::unique_ptr<SmbCredential> credential = std::make_unique<SmbCredential>(
      options->workgroup, options->username, nullptr);
  if (options->kerberos_config) {
    delegate_->SetupKerberos(
        std::move(options->kerberos_config),
        base::BindOnce(&SmbFsBootstrapImpl::OnCredentialsSetup,
                       base::Unretained(this), std::move(options),
                       std::move(smbfs_delegate), callback,
                       std::move(credential), true /* use_kerberos */));
    return;
  }

  if (options->password) {
    credential->password = MakePasswordFromMojoHandle(
        std::move(options->password->fd), options->password->length);
  }

  OnCredentialsSetup(std::move(options), std::move(smbfs_delegate), callback,
                     std::move(credential), false /* use_kerberos */,
                     true /* setup_success */);
}

void SmbFsBootstrapImpl::OnCredentialsSetup(
    mojom::MountOptionsPtr options,
    mojom::SmbFsDelegatePtr smbfs_delegate,
    const MountShareCallback& callback,
    std::unique_ptr<SmbCredential> credential,
    bool use_kerberos,
    bool setup_success) {
  if (!setup_success) {
    callback.Run(mojom::MountError::kUnknown, nullptr);
    return;
  }

  auto fs = delegate_->CreateSmbFilesystem(options->share_path,
                                           std::move(credential));
  // Don't use the resolved address if Kerberos is set up. Kerberos requires the
  // full hostname to obtain auth tickets.
  if (options->resolved_host && !use_kerberos) {
    if (options->resolved_host->address_bytes.size() != 4) {
      LOG(ERROR) << "Invalid IP address size: "
                 << options->resolved_host->address_bytes.size();
      callback.Run(mojom::MountError::kInvalidOptions, nullptr);
      return;
    }
    fs->SetResolvedAddress(options->resolved_host->address_bytes);
  }
  if (!options->skip_connect) {
    SmbFilesystem::ConnectError error = fs->EnsureConnected();
    if (error != SmbFilesystem::ConnectError::kOk) {
      LOG(ERROR) << "Unable to connect to SMB share " << options->share_path
                 << ": " << error;
      callback.Run(ConnectErrorToMountError(error), nullptr);
      return;
    }
  }

  mojom::SmbFsPtr smbfs_ptr;
  fs->SetSmbFsImpl(std::make_unique<SmbFsImpl>(
      fs.get(), std::move(smbfs_delegate), mojo::MakeRequest(&smbfs_ptr)));

  if (!delegate_->StartFuseSession(std::move(fs))) {
    callback.Run(mojom::MountError::kUnknown, nullptr);
    return;
  }

  callback.Run(mojom::MountError::kOk, std::move(smbfs_ptr));
}

}  // namespace smbfs