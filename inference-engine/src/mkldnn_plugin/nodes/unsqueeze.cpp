// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "base.hpp"

#include <cmath>
#include <string>
#include <vector>
#include <cassert>
#include "ie_parallel.hpp"
#include "common/cpu_memcpy.h"

namespace InferenceEngine {
namespace Extensions {
namespace Cpu {

class UnsqueezeImpl: public ExtLayerBase {
public:
    explicit UnsqueezeImpl(const CNNLayer* layer) {
        try {
            if (layer->insData.empty() || layer->outData.empty())
                IE_THROW() << layer->name << " Incorrect number of input/output edges!";

            if (layer->insData.size() != 1 && layer->insData.size() != 2)
                IE_THROW() << layer->name << " Incorrect number of input edges!";

            if (layer->insData.size() == 1)
                addConfig(layer, { { ConfLayout::PLN, false, 0 } }, { { ConfLayout::PLN, false, 0 } });
            else
                addConfig(layer, { { ConfLayout::PLN, false, 0 }, { ConfLayout::PLN, false, 0 } }, { { ConfLayout::PLN, false, 0 } });

            // WA to enable the implementation only for equal input and output precisions
            confs[0].inConfs[0].desc.setPrecision(confs[0].outConfs[0].desc.getPrecision());
        } catch (InferenceEngine::Exception &ex) {
            errorMsg = ex.what();
        }
    }

    StatusCode execute(std::vector<Blob::Ptr>& inputs, std::vector<Blob::Ptr>& outputs, ResponseDesc *resp) noexcept override {
        const uint8_t *src = inputs[0]->cbuffer().as<uint8_t *>() + inputs[0]->getTensorDesc().getBlockingDesc().getOffsetPadding()*inputs[0]->element_size();
        uint8_t* dst = outputs[0]->cbuffer().as<uint8_t *>() + outputs[0]->getTensorDesc().getBlockingDesc().getOffsetPadding()*outputs[0]->element_size();

        if (src != dst) {
            size_t srcSize = inputs[0]->byteSize();
            size_t dstSize = outputs[0]->byteSize();
            cpu_memcpy_s(dst, dstSize, src, srcSize);
        }

        return OK;
    }
};

REG_FACTORY_FOR(UnsqueezeImpl, Unsqueeze);

}  // namespace Cpu
}  // namespace Extensions
}  // namespace InferenceEngine
