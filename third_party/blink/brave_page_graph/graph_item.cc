/* Copyright (c) 2019 The Brave Software Team. Distributed under the MPL2
 * license. This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/brave_page_graph/graph_item.h"
#include <map>
#include <sstream>
#include <string>
#include "brave/third_party/blink/brave_page_graph/graphml.h"
#include "brave/third_party/blink/brave_page_graph/page_graph.h"
#include "brave/third_party/blink/brave_page_graph/types.h"

using ::std::map;
using ::std::string;
using ::std::stringstream;

namespace brave_page_graph {

GraphItem::GraphItem(const PageGraph* graph, const PageGraphId id) :
    graph_(graph),
    id_(id) {}

GraphItem::~GraphItem() {}

GraphMLFuncAttrMap GraphItem::GraphMLAttributeDefs() const {
  return GraphMLFuncAttrMap();
}

string GraphItem::GraphMLAttributes() {
  GraphMLFuncAttrMap mapping = GraphMLAttributeDefs();
  if (mapping.size() == 0) {
    return "";
  }

  stringstream builder;
  for (const auto& elm : mapping) {
    GraphMLValueFunc func = elm.first;
    GraphMLAttr attr = elm.second;
    void* ptr = static_cast<void*>(this);
    string att_value = (*func)(ptr);
    builder << attr.ToAttrString(att_value);
  }
  return builder.str();
}

PageGraphId GraphItem::GetId() const {
  return id_;
}

string GraphItem::ToStringBody() const {
  return ItemName();
}

string GraphItem::ToString() const {
  return ToStringPrefix() + ToStringBody() + ToStringSuffix();
}

}  // namespace brave_page_graph
