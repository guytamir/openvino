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

#include "ngraph/op/sigmoid.hpp"
#include <ngraph/validation_util.hpp>

#include "itt.hpp"
#include "ngraph/log.hpp"
#include "ngraph/util.hpp"

#include "ngraph/runtime/host_tensor.hpp"
#include "ngraph/runtime/reference/sigmoid.hpp"

using namespace std;
using namespace ngraph;

constexpr NodeTypeInfo op::Sigmoid::type_info;

shared_ptr<Node> op::Sigmoid::clone_with_new_inputs(const OutputVector& new_args) const
{
    NGRAPH_OP_SCOPE(v0_Sigmoid_clone_with_new_inputs);
    check_new_args_count(this, new_args);
    return make_shared<Sigmoid>(new_args.at(0));
}

op::Sigmoid::Sigmoid(const Output<Node>& arg)
    : UnaryElementwiseArithmetic(arg)
{
    constructor_validate_and_infer_types();
}

namespace sigmoid
{
    template <element::Type_t ET>
    inline bool evaluate(const HostTensorPtr& arg0, const HostTensorPtr& out, const size_t count)
    {
        using T = typename element_type_traits<ET>::value_type;
        runtime::reference::sigmoid<T>(arg0->get_data_ptr<ET>(), out->get_data_ptr<ET>(), count);
        return true;
    }

    bool evaluate_sigmoid(const HostTensorPtr& arg0, const HostTensorPtr& out)
    {
        bool rc = true;
        size_t count = shape_size(arg0->get_shape());
        out->set_unary(arg0);

        switch (arg0->get_element_type())
        {
            NGRAPH_TYPE_CASE(evaluate_sigmoid, boolean, arg0, out, count);
            NGRAPH_TYPE_CASE(evaluate_sigmoid, i32, arg0, out, count);
            NGRAPH_TYPE_CASE(evaluate_sigmoid, i64, arg0, out, count);
            NGRAPH_TYPE_CASE(evaluate_sigmoid, u32, arg0, out, count);
            NGRAPH_TYPE_CASE(evaluate_sigmoid, u64, arg0, out, count);
            NGRAPH_TYPE_CASE(evaluate_sigmoid, f16, arg0, out, count);
            NGRAPH_TYPE_CASE(evaluate_sigmoid, f32, arg0, out, count);
        default: rc = false; break;
        }
        return rc;
    }
}

bool op::Sigmoid::evaluate(const HostTensorVector& outputs, const HostTensorVector& inputs) const
{
    NGRAPH_OP_SCOPE(v0_Sigmoid_evaluate);
    NGRAPH_CHECK(this,
                 validate_host_tensor_vector(outputs, 1) && validate_host_tensor_vector(inputs, 1));
    return sigmoid::evaluate_sigmoid(inputs[0], outputs[0]);
}
