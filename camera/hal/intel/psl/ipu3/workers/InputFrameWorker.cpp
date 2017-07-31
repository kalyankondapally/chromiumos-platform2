/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "InputFrameWorker"

#include "InputFrameWorker.h"

#include "PerformanceTraces.h"
#include "NodeTypes.h"

namespace android {
namespace camera2 {

const unsigned int INPUTFRAME_WORK_BUFFERS = 7;

InputFrameWorker::InputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId) :
        FrameWorker(node, cameraId, "InputFrameWorker")
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mPollMe = true;
}

InputFrameWorker::~InputFrameWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

status_t InputFrameWorker::configure(std::shared_ptr<GraphConfig> & /*config*/)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    status_t ret = mNode->getFormat(mFormat);
    if (ret != OK)
        return ret;

    ret = setWorkerDeviceBuffers(getDefaultMemoryType(IMGU_NODE_INPUT), INPUTFRAME_WORK_BUFFERS);
    if (ret != OK)
        return ret;

    mIndex = 0;

    return OK;
}

status_t InputFrameWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;
    int memType = mNode->getMemoryType();

    if (memType == V4L2_MEMORY_USERPTR)
        mBuffers[mIndex].m.userptr = (long unsigned int)msg->pMsg.rawNonScaledBuffer->buf->data();
    else if (memType == V4L2_MEMORY_DMABUF) {
        mBuffers[mIndex].m.fd = msg->pMsg.rawNonScaledBuffer->buf->dmaBufFd();
        CheckError((mBuffers[mIndex].m.fd < 0), BAD_VALUE, "@%s invalid fd(%d) passed from isys.\n",
            __func__, mBuffers[mIndex].m.fd);
    } else {
        LOGE("@%s unsupported memory type %d.", __func__, memType);
        return BAD_VALUE;
    }
    status |= mNode->putFrame(&mBuffers[mIndex]);

    msg->pMsg.processingSettings->request->setSeqenceId(msg->pMsg.rawNonScaledBuffer->v4l2Buf.sequence);
    mIndex++;
    mIndex = mIndex % INPUTFRAME_WORK_BUFFERS;
    PERFORMANCE_HAL_ATRACE_PARAM1("seqId", msg->pMsg.rawNonScaledBuffer->v4l2Buf.sequence);

    return status;
}

status_t InputFrameWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    return OK;
}

status_t InputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    v4l2_buffer_info outBuf;
    CLEAR(outBuf);
    status_t status = OK;
    status = mNode->grabFrame(&outBuf);

    return (status < 0) ? status : OK;
}


} /* namespace camera2 */
} /* namespace android */
