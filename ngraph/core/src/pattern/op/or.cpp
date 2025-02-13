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

#include "ngraph/pattern/op/or.hpp"
#include "ngraph/pattern/matcher.hpp"

using namespace std;
using namespace ngraph;

constexpr NodeTypeInfo pattern::op::Or::type_info;

const NodeTypeInfo& pattern::op::Or::get_type_info() const
{
    return type_info;
}

bool pattern::op::Or::match_value(Matcher* matcher,
                                  const Output<Node>& pattern_value,
                                  const Output<Node>& graph_value)
{
    for (auto input_value : input_values())
    {
        auto saved = matcher->start_match();
        if (matcher->match_value(input_value, graph_value))
        {
            auto& pattern_map = matcher->get_pattern_value_map();
            pattern_map[input_value.get_node_shared_ptr()] = graph_value;
            return saved.finish(true);
        }
    }
    return false;
}
