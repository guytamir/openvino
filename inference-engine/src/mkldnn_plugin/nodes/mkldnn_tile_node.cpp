// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "mkldnn_tile_node.h"
#include <legacy/ie_layers.h>
#include <string>
#include <mkldnn_types.h>
#include <mkldnn_extension_utils.h>
#include "common/cpu_memcpy.h"

using namespace mkldnn;
using namespace MKLDNNPlugin;
using namespace InferenceEngine;

MKLDNNTileNode::MKLDNNTileNode(const InferenceEngine::CNNLayerPtr& layer, const mkldnn::engine& eng, MKLDNNWeightsSharing::Ptr &cache) :
        MKLDNNNode(layer, eng, cache) {}

void MKLDNNTileNode::getSupportedDescriptors() {
    auto * tileLayer = dynamic_cast<TileLayer*>(getCnnLayer().get());

    if (tileLayer == nullptr)
        IE_THROW() << "Cannot convert tile layer.";

    if (getParentEdges().size() != 1)
        IE_THROW() << "Incorrect number of input edges for layer " << getName();
    if (!getChildEdges().size())
        IE_THROW() << "Incorrect number of output edges for layer " << getName();

    axis = tileLayer->axis;
    tiles = tileLayer->tiles;
}

void MKLDNNTileNode::initSupportedPrimitiveDescriptors() {
    if (!supportedPrimitiveDescriptors.empty())
        return;

    InferenceEngine::Precision precision = getCnnLayer()->insData[0].lock()->getPrecision();
    if (precision.size() != sizeof(PrecisionTrait<Precision::I32>::value_type) &&
        precision.size() != sizeof(PrecisionTrait<Precision::I16>::value_type) &&
        precision.size() != sizeof(PrecisionTrait<Precision::I8>::value_type)) {
        IE_THROW() << "Layer Tile has unsupported input precision: " << precision;
    }
    auto inputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(precision);

    auto& inDims = getParentEdgeAt(0)->getDims();
    memory::format_tag fmt = MKLDNNMemory::GetPlainFormat(inDims);

    InferenceEngine::LayerConfig config;
    config.dynBatchSupport = true;
    config.inConfs.resize(1);
    config.outConfs.resize(1);
    config.inConfs[0].inPlace = -1;
    config.inConfs[0].constant = false;
    config.inConfs[0].desc = MKLDNNMemoryDesc(getParentEdgeAt(0)->getDims(), inputDataType, fmt);
    config.outConfs[0].inPlace = -1;
    config.outConfs[0].constant = false;
    config.outConfs[0].desc = MKLDNNMemoryDesc(getChildEdgeAt(0)->getDims(), inputDataType, fmt);
    supportedPrimitiveDescriptors.push_back({config, impl_desc_type::unknown, fmt});
}

void MKLDNNTileNode::createPrimitive() {
    auto& dstMemPtr = getChildEdgeAt(0)->getMemoryPtr();
    auto& srcMemPtr = getParentEdgeAt(0)->getMemoryPtr();
    if (!dstMemPtr || !dstMemPtr->GetPrimitivePtr())
        IE_THROW() << "Destination memory didn't allocate.";
    if (!srcMemPtr || !srcMemPtr->GetPrimitivePtr())
        IE_THROW() << "Input memory didn't allocate.";
    if (getSelectedPrimitiveDescriptor() == nullptr)
        IE_THROW() << "Preferable primitive descriptor is not set.";
    if (getParentEdges().size() != 1)
        IE_THROW() << "Incorrect number of input edges for layer " << getName();
}

void MKLDNNTileNode::execute(mkldnn::stream strm) {
    auto& srcMemory = getParentEdgeAt(0)->getMemory();

    const uint8_t* src_ptr = reinterpret_cast<const uint8_t*>(srcMemory.GetPtr());
    uint8_t* dst_ptr = reinterpret_cast<uint8_t*>(getChildEdgeAt(0)->getMemory().GetPtr());

    int m_inner_dim = 1;
    int m_outer_dim = 1;
    memory::dims inDims = srcMemory.GetDims();
    for (int i=0; i < axis; i++ ) m_outer_dim *= inDims[i];
    for (int i=axis; i < inDims.size(); i++ ) m_inner_dim *= inDims[i];
    if (axis > 0) {
        m_outer_dim /= inDims[0];
        m_outer_dim *= batchToProcess();
    } else {
        m_inner_dim /= inDims[0];
        m_inner_dim *= batchToProcess();
    }

    if (m_inner_dim == 1 && m_outer_dim % 8 == 0 && srcMemory.GetDesc().isBlockedCFormat(8)) {
        /*
         * We may enable tile processing directly to appropriate output format (nChw8c)
         */
        m_inner_dim *= 8;
        m_outer_dim /= 8;
    } else if (m_inner_dim == 1 && m_outer_dim % 16 == 0 && srcMemory.GetDesc().isBlockedCFormat(16)) {
        /*
         * We may enable tile processing directly to appropriate output format (nChw16c)
         */
        m_inner_dim *= 16;
        m_outer_dim /= 16;
    }

    m_inner_dim *= srcMemory.GetDesc().GetElementSize();
    for (int i = 0; i < m_outer_dim; ++i) {
        for (int t = 0; t < tiles; ++t) {
            cpu_memcpy(dst_ptr, src_ptr, m_inner_dim);
            dst_ptr += m_inner_dim;
        }
        src_ptr += m_inner_dim;
    }
}

bool MKLDNNTileNode::created() const {
    return getType() == Tile;
}
REG_MKLDNN_PRIM_FOR(MKLDNNTileNode, Tile);
