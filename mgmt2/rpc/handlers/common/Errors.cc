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
#include "Errors.h"

namespace
{ // anonymous namespace

struct RPCRecordErrorCategory : std::error_category {
  const char *name() const noexcept override;
  std::string message(int ev) const override;
};

const char *
RPCRecordErrorCategory::name() const noexcept
{
  return "rpc_handler_config";
}
// READ_ERROR = 1, RECORD_NOT_CONFIG, YAML_READ_ERROR };
std::string
RPCRecordErrorCategory::message(int ev) const
{
  switch (static_cast<rpc::handlers::errors::RecordError>(ev)) {
  case rpc::handlers::errors::RecordError::RECORD_NOT_FOUND:
    return {"Record not found."};
  case rpc::handlers::errors::RecordError::RECORD_NOT_CONFIG:
    return {"Record is not a configuration type."};
  case rpc::handlers::errors::RecordError::RECORD_NOT_METRIC:
    return {"Record is not a metric type."};
  case rpc::handlers::errors::RecordError::INVALID_RECORD_NAME:
    return {"Invalid Record Name."};
  case rpc::handlers::errors::RecordError::VALIDITY_CHECK_ERROR:
    return {"Validity check failed."};
  case rpc::handlers::errors::RecordError::GENERAL_ERROR:
    return {"Error reading the record."};
  case rpc::handlers::errors::RecordError::RECORD_WRITE_ERROR:
    return {"We could not write the record."};
  default:
    return "Rpc server  config error " + std::to_string(ev);
  }
}

const RPCRecordErrorCategory &
get_record_error_category()
{
  static RPCRecordErrorCategory rpcRecordErrorCategory;
  return rpcRecordErrorCategory;
}

} // anonymous namespace

namespace rpc::handlers::errors
{
std::error_code
make_error_code(rpc::handlers::errors::RecordError e)
{
  return {static_cast<int>(e), get_record_error_category()};
}
} // namespace rpc::handlers::errors
