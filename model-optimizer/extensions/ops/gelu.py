"""
 Copyright (C) 2018-2021 Intel Corporation

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
"""

from mo.front.common.partial_infer.elemental import copy_shape_infer
from mo.graph.graph import Graph
from mo.ops.op import Op


class GeLUOP(Op):
    op = 'Gelu'

    def __init__(self, graph: Graph, attrs: dict):
        mandatory_props = {
            'type': self.op,
            'op': self.op,
            'in_ports_count': 1,
            'out_ports_count': 1,
            'version': 'opset7',
            'infer': copy_shape_infer
        }
        super().__init__(graph, mandatory_props, attrs)

    def backend_attrs(self):
        if self.get_opset() == 'opset7':
            return ['approximation_mode']
        else:
            return []
