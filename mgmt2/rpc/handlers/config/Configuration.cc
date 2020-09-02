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
#include "Configuration.h"

#include <system_error> // TODO: remove
#include <string>
#include <string_view>

#include "ConfigErrors.h"
#include "ConfigUtils.h"

#include "tscore/BufferWriter.h"
#include "records/I_RecCore.h"
#include "records/P_RecCore.h"
#include "tscore/Diags.h"

#include "rpc/handlers/common/RecordsUtils.h"
#include "rpc/handlers/common/Errors.h"

namespace rpc::handlers::config
{
static unsigned configRecType = RECT_CONFIG | RECT_LOCAL;

ts::Rv<YAML::Node>
get_config_records(std::string_view const &id, YAML::Node const &params)
{
  using namespace rpc::handlers::utils;
  ts::Rv<YAML::Node> resp;
  std::error_code ec;

  auto check = [](RecT rec_type, std::error_code &ec) {
    if (!REC_TYPE_IS_CONFIG(rec_type)) {
      // we have an issue
      ec = rpc::handlers::errors::RecordError::RECORD_NOT_CONFIG;
      return false;
    }
    return true;
  };

  for (auto &&element : params) {
    auto const &recordName = element.as<std::string>();
    auto &&[node, error]   = get_yaml_record(recordName, check);

    if (error) {
      ec = error;
      break;
    }

    resp.result().push_back(std::move(node));
  }

  if (ec) {
    push_error(ec, resp.errata());
  }

  return resp;
}

ts::Rv<YAML::Node>
get_config_records_regex(std::string_view const &id, YAML::Node const &params)
{
  using namespace rpc::handlers::utils;
  ts::Rv<YAML::Node> resp;
  std::error_code ec;
  for (auto &&element : params) {
    auto const &recordName = element.as<std::string>();
    auto &&[node, error]   = get_yaml_record_regex(recordName, configRecType);

    if (error) {
      ec = error;
      break;
    }
    // node may have more than one.
    for (auto &&n : node) {
      resp.result().push_back(std::move(n));
    }
  }

  if (ec) {
    push_error(ec, resp.errata());
  }

  return resp;
}

ts::Rv<YAML::Node>
get_all_config_records(std::string_view const &id, YAML::Node const &params)
{
  ts::Rv<YAML::Node> resp;
  static constexpr std::string_view all{".*"};
  using namespace rpc::handlers::utils;

  auto &&[node, error] = get_yaml_record_regex(all, configRecType);

  if (error) {
    push_error(error, resp.errata());
    return resp;
  }

  resp.result() = std::move(node);
  return resp;
}

ts::Rv<YAML::Node>
set_config_records(std::string_view const &id, YAML::Node const &params)
{
  ts::Rv<YAML::Node> resp;
  std::error_code ec;

  // we need the type and the udpate type for now.
  using Context = std::tuple<RecDataT, RecUpdateT, RecCheckT, const char *>;

  for (auto const &kv : params) {
    // we need to get the record
    auto const &name  = kv["record_name"].as<std::string>();
    auto const &value = kv["new_value"].as<std::string>();

    Context recordCtx;

    // Get record info first. We will respond with the update status, so we
    // save it.
    const auto ret = RecLookupRecord(
      name.c_str(),
      [](const RecRecord *record, void *data) {
        auto &[dataType, updateType, checkType, pattern] = *static_cast<Context *>(data);
        if (REC_TYPE_IS_CONFIG(record->rec_type)) {
          dataType   = record->data_type;
          updateType = record->config_meta.update_type;
          checkType  = record->config_meta.check_type;
          if (record->config_meta.check_expr) {
            pattern = record->config_meta.check_expr;
          }
        } // if not??
      },
      &recordCtx);

    // make sure if exist. If not, we stop it and do not keep forward.
    if (ret != REC_ERR_OKAY) {
      ec = RPCConfigHandlingError::RecordNotFound;
      break;
    }

    // now set the value.
    auto const &[dataType, updateType, checkType, pattern] = recordCtx;
    bool checkOk{true};

    // run the check only if we have something to check against it.
    if (pattern != nullptr) {
      checkOk = utils::recordValidityCheck(value, checkType, pattern);
    }

    if (checkOk) {
      switch (dataType) {
      case RECD_INT: {
        const auto val = utils::from_string<int>(value, ec);

        if (!ec && RecSetRecordInt(name.c_str(), val, REC_SOURCE_DEFAULT) != REC_ERR_OKAY) {
          ec = RPCConfigHandlingError::SetError;
        }
      } break;
      case RECD_FLOAT: {
        const auto val = utils::from_string<float>(value, ec);

        if (!ec && RecSetRecordFloat(name.c_str(), val, REC_SOURCE_DEFAULT) != REC_ERR_OKAY) {
          ec = RPCConfigHandlingError::SetError;
        }
      } break;
      case RECD_STRING: {
        if (RecSetRecordString(name.c_str(), const_cast<char *>(value.c_str()), REC_SOURCE_DEFAULT) != REC_ERR_OKAY) {
          ec = RPCConfigHandlingError::SetError;
        }
      } break;
      case RECD_COUNTER: {
        const auto val = utils::from_string<int64_t>(value, ec);

        if (!ec && RecSetRecordInt(name.c_str(), val, REC_SOURCE_DEFAULT) != REC_ERR_OKAY) {
          ec = RPCConfigHandlingError::SetError;
        }
      } break;
      default:;
      }
    } else {
      ec = RPCConfigHandlingError::ValidityCheckFailed;
    }

    // for-loop level check. If any issue during the set's then we will not move
    // forward, so stop it right now.
    if (ec) {
      break;
    }

    // Set the response.
    auto &result = resp.result();

    result["record_name"] = name;
    // TODO: make this properly.
    result["update_status"] = static_cast<int>(updateType);
  }

  if (ec) {
    rpc::handlers::utils::push_error(ec, resp.errata());
  }

  return resp;
}
} // namespace rpc::handlers::config
