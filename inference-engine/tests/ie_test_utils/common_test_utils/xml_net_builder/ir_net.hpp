// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <pugixml.hpp>
#include <ie_precision.hpp>
#include <utility>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <iostream>
#include <algorithm>

namespace CommonTestUtils {

class Layer;

class IRNet;

class Port {
public:
    static std::shared_ptr<Port> Create(const std::weak_ptr<Layer> &parent, size_t id,
                                        const std::vector<size_t> &shape,
                                        const std::map<std::string, std::string> &common_attributes = {}) {
        return std::shared_ptr<Port>(new Port(parent, id, shape, common_attributes));
    }

    void serialize(pugi::xml_node &parent) {
        auto port_node = parent.append_child("port");

        // add common attributes
        port_node.append_attribute("id") = m_id;
        for (auto &attr : m_common_attributes) {
            port_node.append_attribute(attr.first.c_str()) = attr.second.c_str();
        }

        // add all input ports (if not scalar) as children
        for (auto dim : m_shape) {
            auto dim_node = port_node.append_child("dim").prepend_child(pugi::node_pcdata);
            dim_node.set_value(std::to_string(dim).c_str());
        }
    }

    void connect(Port &port);

    size_t getId() const {
        return m_id;
    }

    Layer &getLayer() {
        auto locked = m_parent.lock();
        if (!locked) {
            IE_THROW() << "Error getting parent Layer class";
        }
        return *locked;
    }

    void changeCommonAttributeValue(const std::string &attribute_name, const std::string &new_attribute_value) {
        m_common_attributes[attribute_name] = new_attribute_value;
    }

protected:
    size_t m_id = 0;

    std::vector<size_t> m_shape;
    std::weak_ptr<Layer> m_parent;

    using name  = std::string;
    using value = std::string;
    std::map<name, value> m_common_attributes;

private:
    Port(std::weak_ptr<Layer> parent, size_t id, const std::vector<size_t> &shape,
         std::map<std::string, std::string> common_attributes = {})
            : m_id(id),
              m_shape(shape),
              m_parent(std::move(parent)),
              m_common_attributes(std::move(common_attributes)) {}
};

class Layer : public std::enable_shared_from_this<Layer> {
public:
    static std::shared_ptr<Layer> Create(const std::weak_ptr<IRNet> &parent, size_t id,
                                         const std::map<std::string, std::string> &common_attributes = {}) {
        return std::shared_ptr<Layer>(new Layer(parent, id, common_attributes));
    }

    Port &getPortById(int64_t id) const {
        if (id >= m_ports.size()) {
            IE_THROW() << "Out of range: a port with id " << id << " not found";
        }
        return *m_ports[id];
    }

    size_t getLayerId() const {
        return m_id;
    }

    bool operator<(const Layer &layer) const {
        return m_id < layer.getLayerId();
    }

    void changeCommonAttributeValue(const std::string &attribute_name, const std::string &new_attribute_value) {
        m_common_attributes[attribute_name] = new_attribute_value;
    }

    Layer &addSpecificAttributes(const std::string &attributes_group_name,
                                 const std::map<std::string, std::string> &specific_attributes = {}) {
        m_attributes_groups.insert({attributes_group_name, specific_attributes});
        return *this;
    }

    Port &out(size_t id) const {
        return *m_ports[m_output_ports_indexes[id]];
    }

    Port &in(size_t id) const {
        return *m_ports[m_input_ports_indexes[id]];
    }

    Port &operator[](size_t id) const {
        if (id >= m_ports.size()) {
            IE_THROW() << "Out of range: a port with id " << id << " not found";
        }
        return *m_ports[id];
    }

    IRNet &getNetwork() const {
        auto locked = m_parent.lock();
        if (!locked) {
            IE_THROW() << "Error getting parent IRNet class";
        }
        return *locked;
    }

    const std::string &getName() const {
        auto it = m_common_attributes.find("name");
        if (it == m_common_attributes.end()) {
            IE_THROW() << "The layer attribute \"name\" not found.";
        }
        return it->second;
    }

    Layer &addInPort(const std::vector<size_t> &shape,
                     const std::map<std::string, std::string> &common_attributes = {}) {
        m_input_ports_indexes.push_back(addPort(shape, common_attributes));
        return *this;
    }

    Layer &addOutPort(const std::vector<size_t> &shape,
                      const std::map<std::string, std::string> &common_attributes = {}) {
        m_output_ports_indexes.push_back(addPort(shape, common_attributes));
        return *this;
    }

    void serialize(pugi::xml_node &parent) const {
        auto layer_node = parent.append_child("layer");

        // add common attributes
        layer_node.append_attribute("id") = m_id;
        for (auto &attr : m_common_attributes) {
            layer_node.append_attribute(attr.first.c_str()) = attr.second.c_str();
        }

        // add specific attributes
        auto it = m_attributes_groups.find("data");
        if (it != m_attributes_groups.end()) {
            if (!it->second.empty()) {
                auto data_node = layer_node.append_child("data");
                for (auto &attr : it->second) {
                    data_node.append_attribute(attr.first.c_str()) = attr.second.c_str();
                }
            }
        }

        // add all input ports as children
        if (!m_input_ports_indexes.empty()) {
            auto input_node = layer_node.append_child("input");
            for (auto port_id : m_input_ports_indexes) {
                m_ports[port_id]->serialize(input_node);
            }
        }

        // add all output ports as children
        if (!m_output_ports_indexes.empty()) {
            auto output_node = layer_node.append_child("output");
            for (auto port_id : m_output_ports_indexes) {
                m_ports[port_id]->serialize(output_node);
            }
        }

        // add specific attributes
        for (auto group = m_attributes_groups.rbegin(); group != m_attributes_groups.rend(); ++group) {
            if (group->first != "data" && !group->second.empty()) {
                std::stringstream data(group->first);
                std::string line;
                auto root = layer_node;
                while (std::getline(data, line, '/')) {
                    root = root.append_child(line.c_str());
                }
                for (auto &attr : group->second) {
                    layer_node.append_attribute(attr.first.c_str()) = attr.second.c_str();
                }
            }
        }
    }

protected:
    size_t m_id = 0;
    size_t m_latest_port_id = 0;

    using attr_name  = std::string;
    using attr_value = std::string;
    using attr_group_name = std::string;
    using attr_group_value = std::map<attr_name, attr_value>;

    std::map<attr_name, attr_value> m_common_attributes;
    std::map<attr_group_name, attr_group_value> m_attributes_groups;

    std::vector<size_t> m_input_ports_indexes;
    std::vector<size_t> m_output_ports_indexes;
    std::vector<std::shared_ptr<Port>> m_ports;

    size_t addPort(const std::vector<size_t> &shape,
                   const std::map<std::string, std::string> &common_attributes = {}) {
        auto port = Port::Create(shared_from_this(), m_latest_port_id, shape, common_attributes);
        m_ports.emplace_back(port);
        return m_latest_port_id++;
    }

private:
    std::weak_ptr<IRNet> m_parent;

    Layer(std::weak_ptr<IRNet> parent, size_t id, std::map<std::string, std::string> common_attributes = {})
            : m_id(id),
              m_common_attributes(std::move(common_attributes)),
              m_parent(std::move(parent)) {
    }
};

class IRNet : public std::enable_shared_from_this<IRNet> {
public:
    static std::shared_ptr<IRNet> Create(const std::map<std::string, std::string> &common_attributes = {}) {
        return std::shared_ptr<IRNet>(new IRNet(common_attributes));
    }

    Layer &addLayer(const std::map<std::string, std::string> &common_attributes = {}) {
        auto layer = Layer::Create(shared_from_this(), m_latest_layer_id, common_attributes);
        m_layers.emplace_back(layer);
        m_latest_layer_id++;
        return *layer;
    }

    Layer &getLayerByName(const std::string &layer_name) const {
        auto it = std::find_if(m_layers.begin(), m_layers.end(),
                               [&layer_name](const std::shared_ptr<Layer> &layer) {
                                   return layer_name == layer->getName();
                               });
        if (it == m_layers.end()) {
            IE_THROW() << "Out of range: a layer with name " << layer_name << " not found";
        }
        return **it;
    }

    Layer &getLayerById(size_t id) const {
        if (id >= m_layers.size()) {
            IE_THROW() << "Out of range: a layer with id " << id << " not found";
        }
        return *m_layers[id];
    }

    void addEdge(Port &from, Port &to) {
        m_edges.push_back({from, to});
    }

    std::string serialize() const {
        using namespace pugi;
        pugi::xml_document doc;
        auto network_node = doc.append_child("net");

        // add all available attributes into <net>
        for (auto &attr : m_common_attributes) {
            network_node.append_attribute(attr.first.c_str()) = attr.second.c_str();
        }

        // add all layers as children
        auto layers_node = network_node.append_child("layers");
        for (auto &layer : m_layers) {
            layer->serialize(layers_node);
        }

        // add all edges
        if (!m_edges.empty()) {
            auto edges = network_node.append_child("edges");
            for (auto &edge : m_edges) {
                edge.serialize(edges);
            }
        }

        // convert to string
        std::stringstream ss;
        doc.print(ss, "    ");
        return ss.str();
    }

protected:
    size_t m_latest_layer_id = 0;

    using name = std::string;
    using value = std::string;
    std::vector<std::shared_ptr<Layer>> m_layers;

    std::map<name, value> m_common_attributes;

    struct Edge {
        std::reference_wrapper<Port> from;
        std::reference_wrapper<Port> to;

        void serialize(pugi::xml_node &parent) const {
            auto edge_node = parent.append_child("edge");
            edge_node.append_attribute("from-layer") = from.get().getLayer().getLayerId();
            edge_node.append_attribute("from-port") = from.get().getId();
            edge_node.append_attribute("to-layer") = to.get().getLayer().getLayerId();
            edge_node.append_attribute("to-port") = to.get().getId();
        }
    };

    std::vector<Edge> m_edges;

private:
    explicit IRNet(std::map<std::string, std::string> common_attributes = {}) :
            m_common_attributes(std::move(common_attributes)) {
    }
};

///////////////////////////////// IR v10 ///////////////////////////////
class IRBuilder_v10 {
public:
    explicit IRBuilder_v10(const std::string &name) :
            m_ir_net(IRNet::Create({{"name",    name},
                                    {"version", "10"}})) {
    }

    IRBuilder_v10 &AddLayer(const std::string &name, const std::string &type,
                            std::map<std::string, std::string> specific_attributes = {},
                            const std::string &opset = "opset1") {
        m_ir_net->addLayer({{"name",    name},
                            {"type",    type},
                            {"version", opset}});
        m_latest_layer_name = name;
        if (type == "Const") {
            if (specific_attributes.find("offset") == specific_attributes.end()) {
                specific_attributes["offset"] = std::to_string(m_offset);
            }

            if (specific_attributes.find("size") == specific_attributes.end()) {
                IE_THROW() << "Required attribute \"size\" for Const layer is not found";
            }
            m_offset += std::strtol(specific_attributes["size"].c_str(), nullptr, 10);
        }
        if (!specific_attributes.empty()) {
            m_ir_net->getLayerByName(m_latest_layer_name).addSpecificAttributes("data", specific_attributes);
        }
        return *this;
    }

    IRBuilder_v10 &AddOutPort(InferenceEngine::Precision::ePrecision precision, const std::vector<size_t> &shape) {
        m_ir_net->getLayerByName(m_latest_layer_name).addOutPort(shape,
                                                                 {{"precision", InferenceEngine::Precision(
                                                                         precision).name()}});
        return *this;
    }

    IRBuilder_v10 &AddInPort(InferenceEngine::Precision::ePrecision precision, const std::vector<size_t> &shape) {
        m_ir_net->getLayerByName(m_latest_layer_name).addInPort(shape,
                                                                {{"precision", InferenceEngine::Precision(
                                                                        precision).name()}});
        return *this;
    }

    IRBuilder_v10 &AddEdge(Port &from, Port &to) {
        m_ir_net->addEdge(from, to);
        return *this;
    }

    Layer &operator[](const std::string &layer_name) {
        return m_ir_net->getLayerByName(layer_name);
    }

    Layer &getLayer() {
        return m_ir_net->getLayerByName(m_latest_layer_name);
    }

    std::string serialize() {
        return m_ir_net->serialize();
    }

private:
    size_t m_offset = 0;  // for Constant nodes

    std::string m_latest_layer_name;
    std::shared_ptr<IRNet> m_ir_net;
};

///////////////////////////////// IR v6 ///////////////////////////////
class IRBuilder_v6 {
public:
    explicit IRBuilder_v6(const std::string &name) :
            m_ir_net(IRNet::Create({{"name",    name},
                                    {"version", "6"}})) {
    }

    IRBuilder_v6 &
    AddLayer(const std::string &name, const std::string &type, InferenceEngine::Precision::ePrecision precision,
             const std::map<std::string, std::string> &specific_attributes = {}) {
        m_ir_net->addLayer({{"name",      name},
                            {"type",      type},
                            {"precision", InferenceEngine::Precision(precision).name()}});
        m_latest_layer_name = name;
        if (!specific_attributes.empty()) {
            m_ir_net->getLayerByName(m_latest_layer_name).addSpecificAttributes("data", specific_attributes);
        }
        return *this;
    }

    IRBuilder_v6 &AddLayerWeights(size_t size) {
        m_ir_net->getLayerByName(m_latest_layer_name)
                .addSpecificAttributes("weights", {{"offset", std::to_string(m_offset)},
                                                   {"size",   std::to_string(size)}});
        m_offset += size;
        return *this;
    }

    IRBuilder_v6 &AddLayerBiases(size_t offset, size_t size) {
        m_ir_net->getLayerByName(m_latest_layer_name)
                .addSpecificAttributes("biases", {{"offset", std::to_string(offset)},
                                                  {"size",   std::to_string(size)}});
        return *this;
    }

    IRBuilder_v6 &AddBlobs(size_t offset, size_t size) {
        m_ir_net->getLayerByName(m_latest_layer_name)
                .addSpecificAttributes("blobs/custom", {{"offset", std::to_string(offset)},
                                                        {"size",   std::to_string(size)}});
        return *this;
    }

    IRBuilder_v6 &AddOutPort(const std::vector<size_t> &shape) {
        m_ir_net->getLayerByName(m_latest_layer_name).addOutPort(shape);
        return *this;
    }

    IRBuilder_v6 &AddInPort(const std::vector<size_t> &shape) {
        m_ir_net->getLayerByName(m_latest_layer_name).addInPort(shape);
        return *this;
    }

    IRBuilder_v6 &AddEdge(Port &from, Port &to) {
        m_ir_net->addEdge(from, to);
        return *this;
    }

    Layer &operator[](const std::string &layer_name) {
        return m_ir_net->getLayerByName(layer_name);
    }

    Layer &getLayer() {
        return m_ir_net->getLayerByName(m_latest_layer_name);
    }

    std::string serialize() {
        return m_ir_net->serialize();
    }

private:
    size_t m_offset = 0;  // for Constant nodes

    std::string m_latest_layer_name;
    std::shared_ptr<IRNet> m_ir_net;
};

}  // namespace CommonTestUtils
