/* @file
   @section license License

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

#include <tuple>

#include "../config/ConfigErrors.h"
#include "../config/ConfigUtils.h"

#include "records/I_RecCore.h"
#include "records/P_RecCore.h"
#include "tscore/Diags.h"
#include "tscore/Errata.h"

#include <yaml-cpp/yaml.h>

namespace rpc::handlers::utils
{
void push_error(std::error_code const &ec, ts::Errata &errata);
using ValidateRecType = std::function<bool(RecT, std::error_code &)>;
std::tuple<YAML::Node, std::error_code> get_yaml_record(std::string_view name, ValidateRecType check);
std::tuple<YAML::Node, std::error_code> get_config_yaml_record(std::string_view name);

std::tuple<YAML::Node, std::error_code> get_yaml_record_regex(std::string_view name, unsigned recType);

} // namespace rpc::handlers::utils
