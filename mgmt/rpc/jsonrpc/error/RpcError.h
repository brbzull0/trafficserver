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
#include <map>
#include <unordered_map>
#include <cstdint>
#include <cassert>
#include <optional>
#include <system_error>

namespace jsonrpc
{
namespace error
{
  enum class RpcErrorCode {
    // for std::error_code to work, we shouldn't define 0.
    INVALID_REQUEST  = -32600,
    METHOD_NOT_FOUND = -32601,
    INVALID_PARAMS   = -32602,
    INTERNAL_ERROR   = -32603,
    PARSE_ERROR      = -32700,

    // NoError = 0,
    // versionFts::b
    InvalidVersion     = 1,
    InvalidVersionType = 2,
    MissingVersion,
    // method
    InvalidMethodType,
    MissingMethod,

    // params
    InvalidParamType,

    // id
    InvalidIdType,
    NullId = 8,

    // execution errors

    // Internal rpc error when executing the method.
    ExecutionError
  };
  // TODO: force non 0 check
  std::error_code make_error_code(jsonrpc::error::RpcErrorCode e);
} // namespace error

class RpcErrorException : public std::exception
{
  int _code;
  std::string _message;

public:
  RpcErrorException(std::error_code ec) : _code(ec.value()), _message(ec.message()) {}
};

} // namespace jsonrpc

namespace std
{
template <> struct is_error_code_enum<jsonrpc::error::RpcErrorCode> : true_type {
};
} // namespace std
