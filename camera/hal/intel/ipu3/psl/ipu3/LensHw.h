/*
 * Copyright (C) 2014-2018 Intel Corporation
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

#ifndef IPU3_LENSHW_H_
#define IPU3_LENSHW_H_

#include "PlatformData.h"
#include "cros-camera/v4l2_device.h"

namespace cros {
namespace intel {

class MediaController;
class MediaEntity;

/**
 * \class LensHw
 * This class adds the methods that are needed
 * to drive the camera lens using v4l2 commands and custom ioctl.
 *
 */
class LensHw {
public:
    LensHw(int cameraId, std::shared_ptr<MediaController> mediaCtl);
    ~LensHw();

    status_t init();
    status_t setLens(std::shared_ptr<MediaEntity> entity);

    const char* getLensName(void);
    int getCurrentCameraId(void);

    // FOCUS
    int moveFocusToPosition(int position);
    int moveFocusToBySteps(int steps);
    int getFocusPosition(int &position);
    int getFocusStatus(int &status);
    int startAutoFocus(void);
    int stopAutoFocus(void);
    int getAutoFocusStatus(int &status);
    int setAutoFocusRange(int value);
    int getAutoFocusRange(int &value);
    int enableOis(bool enable);

    // ZOOM
    int moveZoomToPosition(int position);
    int moveZoomToBySteps(int steps);
    int getZoomPosition(int &position);
    int moveZoomContinuous(int position);
    /* [END] IHWLensControl overloads, */

    status_t getLatestPosition(int *position, long long unsigned int *time);

private:
    static const int MAX_LENS_NAME_LENGTH = 32;

    struct lensInfo {
        uint32_t index;      //!< V4L2 index
        char name[MAX_LENS_NAME_LENGTH];
    };


private:
    int mCameraId;
    std::shared_ptr<MediaController> mMediaCtl;
    std::shared_ptr<cros::V4L2Subdevice> mLensSubdev;
    struct lensInfo mLensInput;
    int mLastLensPosition;
    bool mCurrentOisState;
    long long unsigned int mLensMovementStartTime; /*!< In useconds */
};  // class LensHW

} // namespace intel
} // namespace cros

#endif  // IPU3_LENSHW_H__
