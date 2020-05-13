// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simplified interface to the ML service. Used to implement the ml_cmdline
// tool.
#include "ml/simple.h"

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/run_loop.h>
#include <mojo/public/cpp/bindings/interface_request.h>

#include "ml/machine_learning_service_impl.h"
#include "ml/mojom/graph_executor.mojom.h"
#include "ml/mojom/machine_learning_service.mojom.h"
#include "ml/mojom/model.mojom.h"
#include "ml/tensor_view.h"

using ::chromeos::machine_learning::mojom::BuiltinModelId;
using ::chromeos::machine_learning::mojom::BuiltinModelSpec;
using ::chromeos::machine_learning::mojom::BuiltinModelSpecPtr;
using ::chromeos::machine_learning::mojom::CreateGraphExecutorResult;
using ::chromeos::machine_learning::mojom::ExecuteResult;
using ::chromeos::machine_learning::mojom::GraphExecutorPtr;
using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::MachineLearningServicePtr;
using ::chromeos::machine_learning::mojom::ModelPtr;
using ::chromeos::machine_learning::mojom::TensorPtr;

namespace ml {
namespace simple {
namespace {

// Creates a 1-D tensor containing a single value
TensorPtr NewSingleValueTensor(const double value) {
  auto tensor(chromeos::machine_learning::mojom::Tensor::New());
  TensorView<double> tensor_view(tensor);
  tensor_view.Allocate();
  tensor_view.GetShape() = {1};
  tensor_view.GetValues() = {value};
  return tensor;
}

}  // namespace

AddResult Add(const double x, const double y) {
  AddResult result = {"Not completed.", -1.0};

  // Create ML Service
  MachineLearningServicePtr ml_service;
  const MachineLearningServiceImpl ml_service_impl(
      mojo::MakeRequest(&ml_service).PassMessagePipe(), base::Closure());

  // Load model.
  BuiltinModelSpecPtr spec = BuiltinModelSpec::New();
  spec->id = BuiltinModelId::TEST_MODEL;
  ModelPtr model;
  bool model_load_ok = false;
  ml_service->LoadBuiltinModel(
      std::move(spec), mojo::MakeRequest(&model),
      base::Bind(
          [](bool* const model_load_ok, const LoadModelResult result) {
            *model_load_ok = result == LoadModelResult::OK;
          },
          &model_load_ok));
  base::RunLoop().RunUntilIdle();
  if (!model_load_ok) {
    result.status = "Failed to load model.";
    return result;
  }

  // Get graph executor for model.
  GraphExecutorPtr graph_executor;
  bool graph_executor_ok = false;
  model->CreateGraphExecutor(mojo::MakeRequest(&graph_executor),
                             base::Bind(
                                 [](bool* const graph_executor_ok,
                                    const CreateGraphExecutorResult result) {
                                   *graph_executor_ok =
                                       result == CreateGraphExecutorResult::OK;
                                 },
                                 &graph_executor_ok));
  base::RunLoop().RunUntilIdle();
  if (!model_load_ok) {
    result.status = "Failed to get graph executor";
    return result;
  }

  // Construct input to graph executor and perform inference
  base::flat_map<std::string, TensorPtr> inputs;
  inputs.emplace("x", NewSingleValueTensor(x));
  inputs.emplace("y", NewSingleValueTensor(y));
  std::vector<std::string> outputs({"z"});
  bool inference_ok = false;
  graph_executor->Execute(
      std::move(inputs), std::move(outputs),
      base::Bind(
          [](bool* const inference_ok, double* const sum,
             const ExecuteResult execute_result,
             base::Optional<std::vector<TensorPtr>> outputs) {
            // Check that the inference succeeded and gave the expected number
            // of outputs.
            *inference_ok = execute_result == ExecuteResult::OK &&
                            outputs.has_value() && outputs->size() == 1;
            if (!*inference_ok) {
              return;
            }

            // Get value from output
            const TensorView<double> out_tensor((*outputs)[0]);
            *sum = out_tensor.GetValues()[0];
          },
          &inference_ok, &result.sum));
  base::RunLoop().RunUntilIdle();
  if (!inference_ok) {
    result.status = "Inference failed.";
    return result;
  }

  result.status = "Inference succeeded.";
  return result;
}

}  // namespace simple
}  // namespace ml