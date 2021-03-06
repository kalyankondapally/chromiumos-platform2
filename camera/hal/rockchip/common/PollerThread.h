/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAMERA3_HAL_POLLERTHREAD_H_
#define CAMERA3_HAL_POLLERTHREAD_H_

#include <string>
#include <vector>

#include <cros-camera/camera_thread.h>

#include "v4l2device.h"

NAMESPACE_DECLARATION {

#define EVENT_POLL_TIMEOUT 100 //100 milliseconds timeout

/**
 * \class IPollEventListener
 *
 * Abstract interface implemented by entities interested on receiving notifications
 * from IPU PollerThread.
 *
 * Notifications are sent whenever the poll returns.
 */
class IPollEventListener {
public:
    enum PollEventMessageId {
        POLL_EVENT_ID_EVENT = 0,
        POLL_EVENT_ID_ERROR
    };

    struct PollEventMessageData {
        const std::vector<std::shared_ptr<V4L2DeviceBase>> *activeDevices;
        const std::vector<std::shared_ptr<V4L2DeviceBase>> *inactiveDevices;
        std::vector<std::shared_ptr<V4L2DeviceBase>> *polledDevices; // NOTE: notified entity is allowed to change this!
        int reqId;
        int pollStatus;
    };

    struct PollEventMessage {
        PollEventMessageId id;
        PollEventMessageData data;
    };

    virtual status_t notifyPollEvent(PollEventMessage *msg) = 0;
    virtual ~IPollEventListener() {};

}; //IPollEventListener

class PollerThread
{
public:
    explicit PollerThread(const char* name);
    ~PollerThread();

    // Public Methods
    status_t init(std::vector<std::shared_ptr<V4L2DeviceBase>> &devices,
                  IPollEventListener *observer,
                  int events = POLLPRI | POLLIN | POLLERR,
                  bool makeRealtime = true);
    status_t pollRequest(int reqId, int timeout = EVENT_POLL_TIMEOUT,
                         std::vector<std::shared_ptr<V4L2DeviceBase>> *devices = nullptr);
    status_t flush(bool sync = false, bool clear = false);
    status_t requestExitAndWait();

//Private Members
private:
    struct MessageInit {
        IPollEventListener *observer;
        int events;
        bool makeRealtime;
        std::vector<std::shared_ptr<V4L2DeviceBase>> devices;
    };

    struct MessageFlush {
        bool clearVectors;
    };

    struct MessagePollRequest {
        unsigned int reqId;
        unsigned int timeout;
        std::vector<std::shared_ptr<V4L2DeviceBase>> devices;
    };

    std::vector<std::shared_ptr<V4L2DeviceBase>> mPollingDevices;
    std::vector<std::shared_ptr<V4L2DeviceBase>> mActiveDevices;
    std::vector<std::shared_ptr<V4L2DeviceBase>> mInactiveDevices;

    std::string mName;
    cros::CameraThread mCameraThread;
    IPollEventListener* mListener; // one listener per PollerThread, PollerThread doesn't has ownership
    int mFlushFd[2];    // Flush file descriptor
    pid_t mPid;
    int mEvents;

//Private Methods
private:
    status_t handleInit(MessageInit msg);
    status_t handlePollRequest(MessagePollRequest msg);
    status_t handleFlush(MessageFlush msg);

    status_t notifyListener(IPollEventListener::PollEventMessage *msg);

};

} NAMESPACE_DECLARATION_END

#endif /* CAMERA3_HAL_POLLERTHREAD_H_ */
