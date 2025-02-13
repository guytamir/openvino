// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "mkldnn_lrn_node.h"

#include <legacy/ie_layers.h>
#include <string>
#include <mkldnn_extension_utils.h>

using namespace mkldnn;
using namespace MKLDNNPlugin;
using namespace InferenceEngine;

MKLDNNLrnNode::MKLDNNLrnNode(const InferenceEngine::CNNLayerPtr& layer, const mkldnn::engine& eng, MKLDNNWeightsSharing::Ptr &cache) :
        MKLDNNNode(layer, eng, cache) {}

void MKLDNNLrnNode::getSupportedDescriptors() {
    if (!descs.empty())
        return;
    InferenceEngine::Precision precision = getCnnLayer()->insData[0].lock()->getPrecision();
    if (precision != InferenceEngine::Precision::FP32 && precision != InferenceEngine::Precision::BF16)
        precision = InferenceEngine::Precision::FP32;
    auto inputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(precision);
    auto * lrnLayer = dynamic_cast<NormLayer*>(getCnnLayer().get());

    if (lrnLayer == nullptr)
        IE_THROW() << "Cannot convert lrn layer.";

    if (getParentEdges().size() != 1)
        IE_THROW() << "Incorrect number of input edges for layer " << getName();
    if (getChildEdges().empty())
        IE_THROW() << "Incorrect number of output edges for layer " << getName();

    isAcrossMaps = lrnLayer->_isAcrossMaps;
    alpha = lrnLayer->_alpha;
    beta = lrnLayer->_beta;
    size = lrnLayer->_size;
    k = lrnLayer->_k;

    auto parentDims = getParentEdgeAt(0)->getDims();

    for (auto format : getAvailableFormatsForDims(parentDims)) {
        MKLDNNMemoryDesc in_candidate(parentDims, inputDataType, format);
        createDescriptor({in_candidate}, {});
    }
}

void MKLDNNLrnNode::createPrimitive() {
    if (prim)
        return;

    auto prim_desc = createPrimitiveDescriptor<lrn_forward::primitive_desc, lrn_forward::desc>();

    prim.reset(new lrn_forward(prim_desc));

    auto src = getParentEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
    auto dst = getChildEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
    primArgs = {{DNNL_ARG_SRC, src}, {DNNL_ARG_DST, dst}};
}

bool MKLDNNLrnNode::created() const {
    return getType() == Lrn;
}

void MKLDNNLrnNode::initOptimalPrimitiveDescriptor() {
    auto selected_pd = getSelectedPrimitiveDescriptor();
    if (selected_pd == nullptr)
        IE_THROW() << "Preferable primitive descriptor is not set.";
    auto config = selected_pd->getConfig();
    if (isInitConfig(config))
        return;

    if (config.inConfs.size() != 1 || config.outConfs.size() != 1 ||
            (!isUninitTensorDesc(config.inConfs[0].desc) &&
                    !isUninitTensorDesc(config.outConfs[0].desc) && config.inConfs[0].desc != config.outConfs[0].desc))
        IE_THROW() << "Layer " << getName() << " has incorrect selected config!";

    if (!isUninitTensorDesc(config.inConfs[0].desc)) {
        config.outConfs[0].desc = config.inConfs[0].desc;
    } else if (!isUninitTensorDesc(config.outConfs[0].desc)) {
        config.inConfs[0].desc = config.outConfs[0].desc;
    } else {
        config.outConfs[0].desc = config.inConfs[0].desc = getConfiguredInputDesc(config, 0);
    }

    initDescriptor(config);
}

void MKLDNNLrnNode::createDescriptor(const std::vector<InferenceEngine::TensorDesc> &inputDesc,
                                     const std::vector<InferenceEngine::TensorDesc> &outputDesc) {
    algorithm alg = (isAcrossMaps) ? algorithm::lrn_across_channels : algorithm::lrn_within_channel;
    MKLDNNMemoryDesc in_candidate(inputDesc[0]);
    MKLDNNDescriptor desc(std::shared_ptr<lrn_forward::desc>(
            new lrn_forward::desc(prop_kind::forward_scoring, alg, in_candidate, size, alpha, beta, k)));
    descs.push_back(desc);
}
REG_MKLDNN_PRIM_FOR(MKLDNNLrnNode, Lrn);
