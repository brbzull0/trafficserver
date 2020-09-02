/**
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
#include "rpc/jsonrpc/JsonRpc.h"

// Admin API Implementation headers.
#include "rpc/handlers/config/Configuration.h"
#include "rpc/handlers/records/Records.h"

namespace rpc::admin
{
void
register_admin_jsonrpc_handlers()
{
  rpc::JsonRpc::instance().register_internal_api();

  // Config
  using namespace rpc::handlers::config;
  rpc::JsonRpc::instance().add_handler("admin_config_get_records", &get_config_records);
  rpc::JsonRpc::instance().add_handler("admin_config_get_records_regex", &get_config_records_regex);
  rpc::JsonRpc::instance().add_handler("admin_config_get_all_records", &get_all_config_records);
  rpc::JsonRpc::instance().add_handler("admin_config_set_records", &set_config_records);

  using namespace rpc::handlers::records;
  rpc::JsonRpc::instance().add_handler("admin_record_get_records_info", &get_records);

  // Metrics

  // host

  // alarms

  // server

  // storage
}
} // namespace rpc::admin