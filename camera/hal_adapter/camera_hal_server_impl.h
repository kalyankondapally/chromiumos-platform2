/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_ADAPTER_CAMERA_HAL_SERVER_IMPL_H_
#define CAMERA_HAL_ADAPTER_CAMERA_HAL_SERVER_IMPL_H_

#include <memory>
#include <vector>

#include <base/files/file_path.h>
#include <base/single_thread_task_runner.h>
#include <base/threading/thread_checker.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "cros-camera/cros_camera_hal.h"
#include "hal_adapter/camera_hal_adapter.h"
#include "mojo/cros_camera_service.mojom.h"

namespace cros {

class CameraMojoChannelManager;

// CameraHalServerImpl is the implementation of the CameraHalServer Mojo
// interface.  It hosts the camera HAL v3 adapter and registers itself to the
// CameraHalDispatcher Mojo proxy in started by Chrome.  Camera clients such
// as Chrome VideoCaptureDeviceFactory and Android cameraserver process connect
// to the CameraHalDispatcher to ask for camera service; CameraHalDispatcher
// proxies the service requests to CameraHalServerImpl.
class CameraHalServerImpl final {
 public:
  CameraHalServerImpl();
  ~CameraHalServerImpl();

  // Initializes the threads and start monitoring the unix domain socket file
  // created by Chrome.
  bool Start();

 private:
  // IPCBridge wraps all the IPC-related calls. Most of its methods should/will
  // be run on IPC thread.
  class IPCBridge : public mojom::CameraHalServer {
   public:
    IPCBridge(CameraHalServerImpl* camera_hal_server,
              CameraMojoChannelManager* mojo_manager);

    ~IPCBridge();

    void Start(CameraHalAdapter* camera_hal_adapter);

    // CameraHalServer Mojo interface implementation.

    void CreateChannel(mojom::CameraModuleRequest camera_module_request) final;

    void SetTracingEnabled(bool enabled) final;

    // Gets a weak pointer of the IPCBridge. This method can be called on
    // non-IPC thread.
    base::WeakPtr<IPCBridge> GetWeakPtr();

   private:
    // Triggered when the HAL server is registered.
    void OnServerRegistered();

    // Connection error handler for the Mojo connection to CameraHalDispatcher.
    void OnServiceMojoChannelError();

    CameraHalServerImpl* camera_hal_server_;

    CameraMojoChannelManager* mojo_manager_;

    // The Mojo IPC task runner.
    const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

    const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

    CameraHalAdapter* camera_hal_adapter_;

    // The CameraHalServer implementation binding.  All the function calls to
    // |binding_| runs on |ipc_task_runner_|.
    mojo::Binding<mojom::CameraHalServer> binding_;

    base::WeakPtrFactory<IPCBridge> weak_ptr_factory_{this};
  };

  // Loads all the camera HAL implementations.
  void LoadCameraHal();

  void ExitOnMainThread(int exit_status);

  std::unique_ptr<CameraMojoChannelManager> mojo_manager_;

  // The instance which deals with the IPC-related calls. It should always run
  // and be deleted on IPC thread.
  std::unique_ptr<IPCBridge> ipc_bridge_;

  // Interfaces of Camera HALs.
  std::vector<cros_camera_hal_t*> cros_camera_hals_;

  // The camera HAL adapter instance.  Each call to CreateChannel creates a
  // new Mojo binding in the camera HAL adapter.  Currently the camera HAL
  // adapter serves two clients: Chrome VideoCaptureDeviceFactory and Android
  // cameraserver process.
  std::unique_ptr<CameraHalAdapter> camera_hal_adapter_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CameraHalServerImpl);
};

}  // namespace cros

#endif  // CAMERA_HAL_ADAPTER_CAMERA_HAL_SERVER_IMPL_H_
