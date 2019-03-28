/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/operators/dgc_op.h"
#include <string>
#include <vector>
#include "paddle/fluid/framework/op_registry.h"

namespace paddle {
namespace operators {

class DGCOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

  void InferShape(framework::InferShapeContext* ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput("U"), "Input(U) of DGCop should not be null.");
    PADDLE_ENFORCE(ctx->HasInput("V"), "Input(V) of DGCop should not be null.");
    PADDLE_ENFORCE(ctx->HasInput("Grad"),
                   "Input(Grad) of DGCop should not be null.");
    PADDLE_ENFORCE(ctx->HasInput("current_step"),
                   "Input(current_step) of DGCop should not be null.");

    PADDLE_ENFORCE(ctx->HasOutput("U_out"),
                   "Output(U_out) of DGCop should not be null.");
    PADDLE_ENFORCE(ctx->HasOutput("V_out"),
                   "Output(V_out) of DGCop should not be null.");
    PADDLE_ENFORCE(ctx->HasOutput("k"),
                   "Output(k) of DGCop should not be null.");
    PADDLE_ENFORCE(ctx->HasOutput("EncodeGrad"),
                   "Output(EncodeGrad) of DGCop should not be null.");
  }

 protected:
  framework::OpKernelType GetKernelTypeForVar(
      const std::string& var_name, const framework::Tensor& tensor,
      const framework::OpKernelType& expected_kernel_type) const override {
    if (var_name == "current_step" || var_name == "rampup_step" ||
        var_name == "k") {
      VLOG(10) << "var_name:" << var_name << " need not to transform";
      return expected_kernel_type;
    }

    return framework::OperatorWithKernel::GetKernelTypeForVar(
        var_name, tensor, expected_kernel_type);
  }
};

class DGCOpMaker : public framework::OpProtoAndCheckerMaker {
 public:
  void Make() override {
    AddInput("U", "(Tensor) Middle tensor of DGC");
    AddInput("V", "(Tensor) Middle tensor of DGC");
    AddInput("Grad", "(Tensor) Input gradient");
    AddInput("current_step", "(Tensor) Current step.");

    AddOutput("U_out",
              "(Tensor) "
              "Output encoded gradient");
    AddOutput("V_out",
              "(Tensor) "
              "Output encoded gradient");
    AddOutput("EncodeGrad",
              "(Tensor) "
              "Output encoded gradient");
    AddOutput("Grad_out",
              "(Tensor) "
              "Output grad gradient");
    AddOutput("k",
              "(Tensor) "
              "Output top-k value");

    AddAttr<float>("m",
                   "(float, 0.9) "
                   "The momentum of learning rate.")
        .SetDefault(0.9);

    AddAttr<bool>("use_nesterov",
                  "(bool, true)"
                  "The momentum of learning rate.")
        .SetDefault(true);

    AddAttr<std::vector<float>>("sparsity",
                                "(vecotr, float)"
                                "The period sparsity of k_select.");

    AddAttr<float>("rampup_begin_step",
                   "(float, 0.0)"
                   "The period when begin k_select.")
        .SetDefault(0.0);

    AddAttr<float>("rampup_step",
                   "(float, 0.0)"
                   "The period when begin k_select.");

    AddComment(R"DOC(
    Original paper is https://arxiv.org/abs/1712.01887

    DGC reduce the communication bandwidth by sending only the important gradients (sparse update):\
        only gradients larger than a threshold are transmitted.

    To avoid losing information, DGC accumulate the rest of the gradients locally.

    Eventually, these gradients become large enough to be transmitted.

    Thus, DGC send the large gradients immediately but eventually send all of the gradients over time.

    To ensure no loss of accuracy, DGC employs momentum correc-tionandlocal gradient clipping on top of the gradient sparsification to maintain model performance.

    DGC also uses momentum factor masking and warmup training to overcome the staleness problem caused by reduced communication.

    This optimizer will do two things:
        
        1. Compress the gradient by get TopK import value from tensor \
            and use it for allreduce to reduce network bandwidth.
    
        2. Call momentum to optimize on the cost.

)DOC");
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;
REGISTER_OP_WITHOUT_GRADIENT(dgc, ops::DGCOp, ops::DGCOpMaker);