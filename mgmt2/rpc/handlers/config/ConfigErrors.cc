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
#include "ConfigErrors.h"

#include <system_error> // TODO: remove
#include <string>

namespace
{ // anonymous namespace

struct RPCConfigHandlingErrorCategory : std::error_category {
  const char *name() const noexcept override;
  std::string message(int ev) const override;
};

const char *
RPCConfigHandlingErrorCategory::name() const noexcept
{
  return "rpc_handler_config";
}
// READ_ERROR = 1, RECORD_NOT_CONFIG, YAML_READ_ERROR };
std::string
RPCConfigHandlingErrorCategory::message(int ev) const
{
  using namespace rpc::handlers::config;

  switch (static_cast<RPCConfigHandlingError>(ev)) {
  case RPCConfigHandlingError::ReadError:
    return {"We cannot read the record"};
  case RPCConfigHandlingError::RecordNotFound:
    return {"We couldn't find a record"};
  case RPCConfigHandlingError::RecordNotConfig:
    return {"Requested Records is not a configuration record"};
  case RPCConfigHandlingError::InvalidRecordName:
    return {"Record name provided is not valid"};
  case RPCConfigHandlingError::ParamParseError:
    return {"Error while parsing the passed parameters"};
  case RPCConfigHandlingError::ValidityCheckFailed:
    return {"The incoming value didn't pass the validation"};
  case RPCConfigHandlingError::SetError:
    return {"We couldn't store the value"};
  default:
    return "Rpc server  config error " + std::to_string(ev);
  }
}

const RPCConfigHandlingErrorCategory &
get_config_handling_error_category()
{
  static RPCConfigHandlingErrorCategory rpcConfigHandlingErrorCategory;
  return rpcConfigHandlingErrorCategory;
}

} // anonymous namespace

namespace rpc::handlers::config
{
std::error_code
make_error_code(rpc::handlers::config::RPCConfigHandlingError e)
{
  return {static_cast<int>(e), get_config_handling_error_category()};
}
} // namespace rpc::handlers::config