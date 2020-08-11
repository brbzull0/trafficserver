/*
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#pragma once

#include <optional>
#include <functional>
#include <variant>
#include <yaml-cpp/yaml.h>

#include "tscore/Errata.h"

namespace jsonrpc
{
class RpcHandlerResponse
{
public:
  YAML::Node result;
  ts::Errata errata;
};

using NotificationHandler = std::function<void(const YAML::Node &)>;
using MethodHandler       = std::function<void(std::string_view const &, const YAML::Node &, RpcHandlerResponse &)>;

// WARNING: This two can extend the lifetime of your object, If you are a temporary object, make sure that your remove it yourself
// from the RPC handlers.
template <typename TClass, typename MemberFunctionT>
MethodHandler
method_handler_from_member_function(MemberFunctionT &&_mf, TClass &&_this)
{
  return std::bind(std::forward<MemberFunctionT>(_mf), std::forward<TClass>(_this), std::placeholders::_1, std::placeholders::_2,
                   std::placeholders::_3);
}

template <typename TClass, typename MemberFunctionT>
NotificationHandler
notification_handler_from_member_function(MemberFunctionT &&_mf, TClass &&_this)
{
  return std::bind(std::forward<MemberFunctionT>(_mf), std::forward<TClass>(_this), std::placeholders::_1);
}

} // namespace jsonrpc
