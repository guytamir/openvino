// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "base.hpp"

#include <cmath>
#include <limits>
#include <cfloat>
#include <string>
#include <vector>
#include <cassert>
#include "ie_parallel.hpp"

namespace InferenceEngine {
namespace Extensions {
namespace Cpu {

class LogSoftmaxImpl: public ExtLayerBase {
public:
    explicit LogSoftmaxImpl(const CNNLayer* layer) {
        try {
            if (layer->insData.empty() || layer->outData.empty())
                IE_THROW() << layer->name << " Incorrect number of input/output edges!";

            if (layer->insData.size() != 1)
                IE_THROW() << layer->name << " Incorrect number of input edges!";

            SizeVector dims = layer->insData[0].lock()->getTensorDesc().getDims();
            if (!dims.size())
                dims = SizeVector(1, 1);
            int axis = layer->GetParamAsInt("axis", -1);
            if (axis < 0)
                axis += dims.size();

            if (dims.size() < static_cast<size_t>((size_t)(1) + axis))
                IE_THROW() << layer->name << " Incorrect input parameters dimensions and axis number!";

            int j;
            for (j = dims.size() - 1; j >= 0; j--) {
                if (dims[j] != 1) break;
            }
            if (j == axis) is_last_dim = true;

            for (int i = 0; i < axis; i++)
                axis_step *= dims[i];
            reduced_axis_size = dims[axis];
            for (size_t i = (axis + 1); i < dims.size(); i++)
                reduced_axis_stride *= dims[i];

            addConfig(layer, { { ConfLayout::PLN, false, 0, Precision::FP32 } }, { { ConfLayout::PLN, false, 0, Precision::FP32 } });
        } catch (InferenceEngine::Exception &ex) {
            errorMsg = ex.what();
        }
    }

    StatusCode execute(std::vector<Blob::Ptr>& inputs, std::vector<Blob::Ptr>& outputs, ResponseDesc *resp) noexcept override {
        const float *src_data = inputs[0]->cbuffer().as<float *>() +
            inputs[0]->getTensorDesc().getBlockingDesc().getOffsetPadding();
        float* dst_data = outputs[0]->buffer().as<float *>() +
            outputs[0]->getTensorDesc().getBlockingDesc().getOffsetPadding();

        if (is_last_dim) {
            parallel_for(axis_step, [&](size_t i) {
                const float *src_dataPtr = &src_data[i * reduced_axis_size];
                float *dst_dataPtr = &dst_data[i * reduced_axis_size];

                float reduce_prod = 0.0f;
                const float max = *std::max_element(src_dataPtr, src_dataPtr + reduced_axis_size);
                for (size_t j = 0; j < reduced_axis_size; ++j)
                    reduce_prod += expf(src_dataPtr[j] - max);

                reduce_prod = logf(reduce_prod);
                for (size_t j = 0; j < reduced_axis_size; ++j)
                    dst_dataPtr[j] = src_dataPtr[j] - max - reduce_prod;
            });
        } else {
            parallel_for2d(axis_step, reduced_axis_stride, [&](size_t k, size_t i) {
                const float *src_dataPtr = &src_data[k * reduced_axis_stride * reduced_axis_size + i];
                float *dst_dataPtr = &dst_data[k * reduced_axis_stride * reduced_axis_size + i];

                float reduce_prod = 0.0f;
                float max = std::numeric_limits<float>::min();
                for (size_t j = 0; j < reduced_axis_size; ++j) {
                    if (src_dataPtr[j * reduced_axis_stride] > max)
                        max = src_dataPtr[j * reduced_axis_stride];
                }

                for (size_t j = 0; j < reduced_axis_size; ++j)
                    reduce_prod += expf(src_dataPtr[j * reduced_axis_stride] - max);

                reduce_prod = logf(reduce_prod);
                for (size_t j = 0; j < reduced_axis_size; ++j)
                    dst_dataPtr[j * reduced_axis_stride] = src_dataPtr[j * reduced_axis_stride] - max - reduce_prod;
            });
        }

        return OK;
    }

private:
    size_t reduced_axis_size;
    size_t reduced_axis_stride = 1;
    size_t axis_step = 1;
    bool is_last_dim = false;
};

REG_FACTORY_FOR(LogSoftmaxImpl, LogSoftmax);

}  // namespace Cpu
}  // namespace Extensions
}  // namespace InferenceEngine
