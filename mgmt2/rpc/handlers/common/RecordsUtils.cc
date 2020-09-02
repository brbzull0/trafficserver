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
#include "RecordsUtils.h"
#include "Errors.h"

#include "convert.h"

// check if the config record is overridable.
// #include "shared/overridable_txn_vars.h"

#include <system_error> // TODO: remove
#include <string>

namespace rpc::handlers::utils
{
void
push_error(std::error_code const &ec, ts::Errata &errata)
{
  static const int DefaultErrataId{3};
  errata.push(DefaultErrataId, ec.value(), ec.message());
}

struct Context {
  using CbType = std::function<bool(RecT, std::error_code &)>;
  YAML::Node yaml;
  std::error_code ec;
  // regex do not need to set the callback.
  CbType checkCb;
};
void static get_record_impl(std::string_view name, Context &ctx)
{
  auto yamlConverter = [](const RecRecord *record, void *data) {
    auto &ctx = *static_cast<Context *>(data);

    if (!record) {
      ctx.ec = rpc::handlers::errors::RecordError::RECORD_NOT_FOUND;
      return;
    }

    if (!ctx.checkCb(record->rec_type, ctx.ec)) {
      return;
    }
    // convert it.
    try {
      ctx.yaml = *record;
    } catch (std::exception const &ex) {
      ctx.ec = rpc::handlers::errors::RecordError::GENERAL_ERROR;
      return;
    }
  };

  const auto ret = RecLookupRecord(name.data(), yamlConverter, &ctx);

  if (ctx.ec) {
    return;
  }

  if (ret != REC_ERR_OKAY) {
    // append
    Error("We couldn't get the record %s", name.data());
    ctx.ec = rpc::handlers::errors::RecordError::RECORD_NOT_FOUND;
    return;
  }
}

std::tuple<YAML::Node, std::error_code>
get_yaml_record(std::string_view name, ValidateRecType check)
{
  Context ctx;

  ctx.checkCb = check; //[](RecT rec_type, std::error_code &ec) -> bool { return true; };

  // send this back.
  get_record_impl(name, ctx);

  return {ctx.yaml, ctx.ec};
}

std::tuple<YAML::Node, std::error_code>
get_config_yaml_record(std::string_view name)
{
  Context ctx;

  ctx.checkCb = [](RecT rec_type, std::error_code &ec) -> bool {
    if (!REC_TYPE_IS_CONFIG(rec_type)) {
      // we have an issue
      ec = rpc::handlers::errors::RecordError::GENERAL_ERROR;
      return false;
    }
    return true;
  };

  // send this back.
  get_record_impl(name, ctx);

  return {ctx.yaml, ctx.ec};
}

void static get_record_regex_impl(std::string_view name, unsigned recType, Context &ctx)
{
  auto yamlConverter = [](const RecRecord *record, void *data) {
    auto &ctx = *static_cast<Context *>(data);

    if (!record) {
      return;
    }

    YAML::Node recordYaml;
    // convert it.
    try {
      recordYaml = *record;
    } catch (std::exception const &ex) {
      ctx.ec = rpc::handlers::errors::RecordError::GENERAL_ERROR;
      return;
    }

    // we have to append the records to the context one.
    ctx.yaml.push_back(recordYaml);
  };

  const auto ret = RecLookupMatchingRecords(recType, name.data(), yamlConverter, &ctx);
  if (ctx.ec) {
    return;
  }

  if (ret != REC_ERR_OKAY) {
    // append
    Error("We couldn't get the records that match '%s'", name.data());
    ctx.ec = rpc::handlers::errors::RecordError::GENERAL_ERROR;
    return;
  }
}

std::tuple<YAML::Node, std::error_code>
get_yaml_record_regex(std::string_view name, unsigned recType)
{
  Context ctx;

  get_record_regex_impl(name, recType, ctx);

  return {ctx.yaml, ctx.ec};
}

} // namespace rpc::handlers::utils
