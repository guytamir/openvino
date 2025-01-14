// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <transformations_visibility.hpp>
#include <ngraph/pass/graph_rewrite.hpp>

namespace ngraph {
namespace pass {

class TRANSFORMATIONS_API ConvToBinaryConv;

} // namespace pass
} // namespace ngraph

/**
 * @ingroup ie_transformation_common_api
 * @brief This transformation converts Convolution to BinaryConvolution under following conditions:
 *  - first input to Convolution is a FakeQuantize with levels==2 with output low,high being either (0, 1) or (-1, 1)
 *  - second input (weights) is a constant with values -1 or 1
 * The transformation also converts weights to binary Constant (with 'u1' type)
 * For example, when output_low is equal to 0 and output_high is equal to 1, following graph
 *
 *         .... ....  out_low   out_high
 *           |    |      |         |
 *          +--------------------------+           +-------------------------------------+
 *          | FakeQuantize (levels==2) |           |               Constant              |
 *          |     (on activations)     |           | (weights containing -1 or 1 values) |
 *          +--------------------------+           +-------------------------------------+
 *                        |                                      |
 *                        |                                      |
 *                        -----------------    -------------------
 *                                        |    |
 *                                        v    v
 *                                   +-------------+
 *                                   | Convolution |
 *                                   +-------------+
 *                                          |
 *                                          v
 * is transformed to:
 *
 *         .... ....  out_low   out_high
 *           |    |      |         |
 *          +--------------------------+           +---------------------------------+
 *          | FakeQuantize (levels==2) |           |     Constant (with u1 type)     |
 *          |     (on activations)     |           | (with u1 type - binary weights) |
 *          +--------------------------+           +---------------------------------+
 *                        |                                      |
 *                        |                                      |
 *                        -----------------    -------------------
 *                                        |    |
 *                                        v    v
 *                                +-------------------+
 *                                | BinaryConvolution |
 *                                +-------------------+
 *                                          |
 *                                          v
 *                                   +------------+     +----------------------------------------------------+
 *                                   |            |     |                   Constant                         |
 *                                   |     Add    | <---|          (weights from original graph,             |
 *                                   |            |     |  sum-reduced over [1,..., len(weights.shape)] axes |
 *                                   +------------+     +----------------------------------------------------+
 *                                          |
 *                                          v
 *                                   +------------+     +-----+
 *                                   |  Multiply  | <---| 0.5 |
 *                                   +------------+     +-----+
 *                                          |
 *                                          v
 */
class ngraph::pass::ConvToBinaryConv : public ngraph::pass::MatcherPass {
public:
    NGRAPH_RTTI_DECLARATION;
    ConvToBinaryConv();
};
