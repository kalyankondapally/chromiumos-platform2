// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ml/model_metadata.h"

namespace ml {

using ::chromeos::machine_learning::mojom::ModelId;

std::map<ModelId, ModelMetadata> GetModelMetadata() {
  return {
    {
      ModelId::TEST_MODEL, {
        ModelId::TEST_MODEL,
        "mlservice-model-test_add-20180914.tflite",
        {{"x", 1}, {"y", 2}},
        {{"z", 0}},
      },
    },
    {
      ModelId::SMART_DIM_20181115, {
        ModelId::SMART_DIM_20181115,
        "mlservice-model-smart_dim-20181115.tflite",
        {{"input", 3}},
        {{"output", 4}},
      },
    },
    {
      ModelId::SMART_DIM_20190221, {
        ModelId::SMART_DIM_20190221,
        "mlservice-model-smart_dim-20190221.tflite",
        {{"input", 3}},
        {{"output", 4}},
      },
    },
  };
}

}  // namespace ml
