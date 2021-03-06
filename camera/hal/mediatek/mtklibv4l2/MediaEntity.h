/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MEDIAENTITY_H_
#define CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MEDIAENTITY_H_

#include "Errors.h"
#include "CommonUtilMacros.h"
#include "cros-camera/v4l2_device.h"
#include <mtkcam/def/common.h>

#include <linux/media.h>

#include <memory>
#include <vector>

using cros::V4L2Device;
using cros::V4L2Subdevice;
using cros::V4L2VideoNode;

enum V4L2DeviceType {
  DEVICE_VIDEO,   /*!< MEDIA_ENT_T_DEVNODE_V4L */
  SUBDEV_GENERIC, /*!< MEDIA_ENT_T_V4L2_SUBDEV */
  SUBDEV_SENSOR,  /*!< MEDIA_ENT_T_V4L2_SUBDEV_SENSOR */
  SUBDEV_FLASH,   /*!< MEDIA_ENT_T_V4L2_SUBDEV_FLASH */
  SUBDEV_LENS,    /*!< MEDIA_ENT_T_V4L2_SUBDEV_LENS */
  UNKNOWN_TYPE
};

/**
 * \class MediaEntity
 *
 * This class models a media entity, which is a basic media hardware or software
 * building block (e.g. sensor, scaler, CSI-2 receiver).
 *
 * Each media entity has one or more pads and links. A pad is a connection
 * endpoint through which an entity can interact with other entities. Data
 * produced by an entity flows from the entity's output to one or more entity
 * inputs. A link is a connection between two pads, either on the same entity or
 * on different entities. Data flows from a source pad to a sink pad.
 */
class VISIBILITY_PUBLIC MediaEntity {
 public:
  MediaEntity(const struct media_entity_desc& entity,
              struct media_link_desc* links,
              struct media_pad_desc* pads);
  ~MediaEntity();

  NSCam::status_t getDevice(std::shared_ptr<V4L2Device>* device);
  void updateLinks(const struct media_link_desc* links);
  V4L2DeviceType getType();
  void getLinkDesc(std::vector<media_link_desc>* links) const {
    *links = mLinks;
  }
  void getEntityDesc(struct media_entity_desc* entity) const {
    *entity = mInfo;
  }
  void getPadDesc(struct media_pad_desc* pad, int index) const {
    *pad = mPads[index];
  }
  const char* getName() const { return mInfo.name; }
  int getEntityid() const { return mInfo.id; }

 private:
  NSCam::status_t openDevice(std::shared_ptr<V4L2Device>* device);

 private:
  struct media_entity_desc mInfo; /*!< media entity descriptor info */
  std::vector<struct media_link_desc> mLinks; /*!< media entity links */
  std::vector<struct media_pad_desc> mPads;   /*!< media entity pads */

  std::shared_ptr<V4L2Device> mDevice; /*!< V4L2 video node or subdevice */
};                                     // class MediaEntity

#endif  // CAMERA_HAL_MEDIATEK_MTKLIBV4L2_MEDIAENTITY_H_
