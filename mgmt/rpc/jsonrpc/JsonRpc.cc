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

#include "JsonRpc.h"

#include <iostream>
#include <chrono>
#include <system_error>

#include "json/YAMLUtils.h"

namespace jsonrpc
{
static constexpr auto logTag = "rpc";

RPC::Distpatcher::response_type
RPC::Distpatcher::distpatch(RpcRequestInfo const &request) const
{
  if (request.is_notification()) {
    std::error_code ec;
    invoke_notification_handler(request, ec);
    if (ec) {
      return {std::nullopt, ec};
    }
  } else {
    return invoke_handler(request);
  }

  return response_type{};
}

std::pair<jsonrpc::MethodHandler, bool>
RPC::Distpatcher::find_handler(const std::string &methodName) const
{
  std::lock_guard<std::mutex> lock(_mutex);

  const auto found = std::find_if(std::begin(_methods), std::end(_methods), [&](auto const &p) { return p.first == methodName; });
  return found != std::end(_methods) ? std::make_pair(found->second, true) : std::make_pair(nullptr, false);
}

std::pair<jsonrpc::NotificationHandler, bool>
RPC::Distpatcher::find_notification_handler(const std::string &notificationName) const
{
  std::lock_guard<std::mutex> lock(_mutex);

  const auto found =
    std::find_if(std::begin(_notifications), std::end(_notifications), [&](auto const &p) { return p.first == notificationName; });
  return found != std::end(_notifications) ? std::make_pair(found->second, true) : std::make_pair(nullptr, false);
}

RPC::Distpatcher::response_type
RPC::Distpatcher::invoke_handler(RpcRequestInfo const &request) const
{
  auto &&[handler, found] = find_handler(request.method);
  if (!found || !handler) {
    return {std::nullopt, error::RpcErrorCode::METHOD_NOT_FOUND};
  }

  RpcResponseInfo response{request.id};

  try {
    handler(*request.id, request.params, response.callResult);
  } catch (std::exception const &e) {
    Debug(logTag, "Ups, something happened during the callback invocation: %s", e.what());
    return {std::nullopt, error::RpcErrorCode::INTERNAL_ERROR};
  }

  return {response, {}};
}

void
RPC::Distpatcher::invoke_notification_handler(RpcRequestInfo const &notification, std::error_code &ec) const
{
  auto const &[handler, found] = find_notification_handler(notification.method);
  if (!found) {
    ec = error::RpcErrorCode::METHOD_NOT_FOUND;
    return;
  }
  try {
    handler(notification.params);
  } catch (std::exception const &e) {
    Debug(logTag, "Ups, something happened during the callback(notification) invocation: %s", e.what());
    // it's a notification so we do not care much.
  }
}

bool
RPC::Distpatcher::remove_handler(std::string const &name)
{
  std::lock_guard<std::mutex> lock(_mutex);
  auto foundIt = std::find_if(std::begin(_methods), std::end(_methods), [&](auto const &p) { return p.first == name; });
  if (foundIt != std::end(_methods)) {
    _methods.erase(foundIt);
    return true;
  }

  return false;
}

bool
RPC::Distpatcher::remove_notification_handler(std::string const &name)
{
  std::lock_guard<std::mutex> lock(_mutex);
  auto foundIt = std::find_if(std::begin(_notifications), std::end(_notifications), [&](auto const &p) { return p.first == name; });
  if (foundIt != std::end(_notifications)) {
    _notifications.erase(foundIt);
    return true;
  }

  return false;
}

// RPC

void
RPC::register_internal_api()
{
  // register. TMP
  if (!_distpather.add_handler("show_registered_handlers",
                               method_handler_from_member_function(&Distpatcher::show_registered_handlers, &_distpather))) {
    Warning("Something happened, we couldn't register the rpc show_registered_handlers handler");
  }
}

bool
RPC::remove_handler(std::string const &name)
{
  return _distpather.remove_handler(name);
}

bool
RPC::remove_notification_handler(std::string const &name)
{
  return _distpather.remove_notification_handler(name);
}

static inline RpcResponseInfo
make_error_response(RpcRequestInfo const &req, std::error_code const &ec)
{
  RpcResponseInfo resp;

  // we may have been able to collect the id, if so, use it.
  if (req.id) {
    resp.id = req.id;
  }

  resp.rpcError = std::make_optional<RpcError>(ec.value(), ec.message());

  return resp;
}

static inline RpcResponseInfo
make_error_response(std::error_code const &ec)
{
  RpcResponseInfo resp;

  resp.rpcError = std::make_optional<RpcError>(ec.value(), ec.message());
  return resp;
}

std::optional<std::string>
RPC::handle_call(std::string_view request)
{
  Debug(logTag, "Incoming request '%s'", request.data());

  std::error_code ec;
  try {
    // let's decode all the incoming messages into our own types.
    RpcRequest const &msg = Decoder::extract(request, ec);

    // If any error happened within the request, they will be kept inside each
    // particular request, as they would need to be converted back in a proper error response.
    if (ec) {
      auto response = make_error_response(ec);
      return Encoder::encode(response);
    }

    RpcResponse response{msg.is_batch()};
    for (auto const &[req, decode_error] : msg.get_messages()) {
      // As per jsonrpc specs we do care about invalid messages as long as they are well-formed,  our decode logic will make their
      // best to build up a request, if any errors were detected during decoding, we will save the error and make it part of the
      // RPCRequest elements for further use.
      if (!decode_error) {
        // request seems ok and ready to be dispatched. The dispatcher will tell us if the method exist and if so, it will dispatch
        // the call and gives us back the response.
        auto &&[encodedResponse, ec] = _distpather.distpatch(req);

        // On any error, ec will have a value
        if (!ec) {
          // we only get valid responses if it was a method request, not
          // for notifications.
          if (encodedResponse) {
            response.add_message(*encodedResponse);
          }
        } else {
          // get an error response, we may have the id, so let's try to use it.
          response.add_message(make_error_response(req, ec));
        }

      } else {
        // If the request was marked as error(decode error), we still need to send the error back, so we save it.
        response.add_message(make_error_response(req, decode_error));
      }
    }

    // We will not have a response for notification(s); This could be a batch of notifications only.
    return response.is_notification() ? std::nullopt : std::make_optional<std::string>(Encoder::encode(response));

  } catch (std::exception const &ex) {
    ec = jsonrpc::error::RpcErrorCode::INTERNAL_ERROR;
  }

  return {Encoder::encode(make_error_response(ec))};
}

void
RPC::Distpatcher::show_registered_handlers(std::string_view const &, const YAML::Node &, RpcHandlerResponse &resp)
{
  // use the same lock?
  std::lock_guard<std::mutex> lock(_mutex);
  for (auto const &m : _methods) {
    std::string sm = m.first;
    resp.result["methods"].push_back(sm);
  }

  for (auto const &m : _notifications) {
    std::string sm = m.first;
    resp.result["notifications"].push_back(sm);
  }
}
} // namespace jsonrpc