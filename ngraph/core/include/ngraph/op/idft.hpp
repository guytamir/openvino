//*****************************************************************************
// Copyright 2017-2021 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#pragma once

#include <cstddef>
#include <vector>
#include "ngraph/attribute_adapter.hpp"
#include "ngraph/op/op.hpp"
#include "ngraph/op/util/attr_types.hpp"

namespace ngraph
{
    namespace op
    {
        namespace v7
        {
            /// \brief An operation IDFT that computes the inverse discrete Fourier transformation.
            class NGRAPH_API IDFT : public Op
            {
            public:
                NGRAPH_RTTI_DECLARATION;
                IDFT() = default;

                /// \brief Constructs a IDFT operation. IDFT is performed for full size axes.
                ///
                /// \param data  Input data
                /// \param axes Axes to perform IDFT
                IDFT(const Output<Node>& data, const Output<Node>& axes);

                /// \brief Constructs a IDFT operation.
                ///
                /// \param data  Input data
                /// \param axes Axes to perform IDFT
                /// \param signal_size Signal sizes for 'axes'
                IDFT(const Output<Node>& data,
                     const Output<Node>& axes,
                     const Output<Node>& signal_size);

                bool visit_attributes(AttributeVisitor& visitor) override;

                void validate_and_infer_types() override;

                std::shared_ptr<Node>
                    clone_with_new_inputs(const OutputVector& new_args) const override;

            protected:
                void validate();
            };
        }
    }
}
