/** @file

  Functions for interfacing to management records

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

#include <string_view>
#include <string>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ConfigErrors.h"
// TODO: replace by I_RecCode?
#include "records/I_RecCore.h"

namespace rpc::handlers::config::utils
{
// if ec true, there is no guarantee of the value in T.
template <typename T>
T
from_string(std::string_view value, std::error_code &ec)
{
  T ret;
  try {
    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int64_t>) {
      ret = std::stoi(value.data());
    } else if constexpr (std::is_same_v<T, float>) {
      ret = std::stof(value.data());
    }
  } catch (std::invalid_argument const &e) {
    ec = RPCConfigHandlingError::InvalidFieldValue;
  } catch (std::out_of_range const &e) {
    ec = std::make_error_code(static_cast<std::errc>(errno));
  }

  return ret;
}

bool recordValidityCheck(std::string_view value, RecCheckT checkType, std::string_view pattern);
bool recordRegexCheck(std::string_view pattern, std::string_view value);
bool recordRangeCheck(std::string_view pattern, std::string_view value);
bool recordIPCheck(std::string_view pattern, std::string_view value);

} // namespace rpc::handlers::config::utils