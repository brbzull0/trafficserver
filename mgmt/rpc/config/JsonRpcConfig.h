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

#include <string>
#include <string_view>
#include <variant>

#include "yaml-cpp/yaml.h"

#include "tscore/Diags.h"

namespace jsonrpc::config
{
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

class RPCConfig
{
public:
  enum class TransportType { UNIX_DOMAIN_SOCKET = 0 };

  RPCConfig() = default;

  void
  load(YAML::Node const &params)
  {
    try {
      // TODO: do it properly. default values for now.
      _selectedTransportType = static_cast<TransportType>(params["transport_type"].as<int>());
      _transportConfig       = params["transport_config"];
    } catch (YAML::Exception const &ex) {
      Warning("We found an issue when reading the parameter: %s", ex.what());
    }
  }

  // this will throw.
  YAML::Node
  get_transport_config_params() const
  {
    return _transportConfig;
  }

  TransportType
  get_configured_type() const
  {
    return _selectedTransportType;
  }

private:
  // TODO: consider having a wrap around the node.
  YAML::Node _transportConfig;
  TransportType _selectedTransportType{TransportType::UNIX_DOMAIN_SOCKET};
};
} // namespace jsonrpc::config
