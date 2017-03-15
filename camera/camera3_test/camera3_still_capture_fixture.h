// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_STILL_CAPTURE_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_STILL_CAPTURE_FIXTURE_H_

#include <semaphore.h>
#include <stdio.h>

#include <exif-data.h>
#include <jpeglib.h>

#include "camera3_test/camera3_preview_fixture.h"

namespace camera3_test {

class Camera3StillCaptureFixture : public Camera3PreviewFixture {
 public:
  explicit Camera3StillCaptureFixture(std::vector<int> cam_ids)
      : Camera3PreviewFixture(cam_ids), cam_ids_(cam_ids) {}

  virtual void SetUp() override;

  // Process still capture result metadata and output buffer. Tests can
  // override this function to handle the results to suit their purpose. Note
  // that the metadata |metadata| and output buffer |buffer| will be freed
  // after returning from this call.
  virtual void ProcessStillCaptureResult(int cam_id,
                                         uint32_t frame_number,
                                         CameraMetadataUniquePtr metadata,
                                         BufferHandleUniquePtr buffer);

  // Wait for still capture result with timeout
  int WaitStillCaptureResult(int cam_id, const struct timespec& timeout);

 protected:
  struct StillCaptureResult {
    sem_t capture_result_sem;

    std::vector<CameraMetadataUniquePtr> result_metadatas;

    std::vector<time_t> result_date_time;

    std::vector<BufferHandleUniquePtr> buffer_handles;

    StillCaptureResult();

    ~StillCaptureResult();
  };

  std::unordered_map<int, StillCaptureResult> still_capture_results_;

  // Max JPEG size with camera device id as the index
  std::unordered_map<int, size_t> jpeg_max_sizes_;

  struct JpegExifInfo {
    const BufferHandleUniquePtr& buffer_handle;
    size_t buffer_size;
    void* buffer_addr;
    ResolutionInfo jpeg_resolution;
    ExifData* exif_data;
    JpegExifInfo(const BufferHandleUniquePtr& buffer, size_t size);
    ~JpegExifInfo();
    bool Initialize();
  };

 private:
  std::vector<int> cam_ids_;

  DISALLOW_COPY_AND_ASSIGN(Camera3StillCaptureFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_STILL_CAPTURE_FIXTURE_H_
