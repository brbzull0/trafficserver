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
#include "RecordsUtils.h"

#include <system_error>
#include <string>

#include "convert.h"
#include "records/P_RecCore.h"
#include "tscore/Tokenizer.h"

namespace
{ // anonymous namespace

struct RPCRecordErrorCategory : std::error_category {
  const char *name() const noexcept override;
  std::string message(int ev) const override;
};

const char *
RPCRecordErrorCategory::name() const noexcept
{
  return "rpc_handler_record_error";
}
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
    return "Record error error " + std::to_string(ev);
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

namespace rpc::handlers::records::utils
{
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

// Basic functions to helps setting a record value properly. All this functions were originally in the WebMgmtUtils.
namespace
{ // anonymous namespace
  bool
  recordRegexCheck(std::string_view pattern, std::string_view value)
  {
    pcre *regex;
    const char *error;
    int erroffset;

    regex = pcre_compile(pattern.data(), 0, &error, &erroffset, nullptr);
    if (!regex) {
      return false;
    } else {
      int r = pcre_exec(regex, nullptr, value.data(), value.size(), 0, 0, nullptr, 0);

      pcre_free(regex);
      return (r != -1) ? true : false;
    }

    return false; // no-op
  }

  bool
  recordRangeCheck(std::string_view pattern, std::string_view value)
  {
    char *p = const_cast<char *>(pattern.data());
    Tokenizer dashTok("-");

    if (recordRegexCheck("^[0-9]+$", value)) {
      while (*p != '[') {
        p++;
      } // skip to '['
      if (dashTok.Initialize(++p, COPY_TOKS) == 2) {
        int l_limit = atoi(dashTok[0]);
        int u_limit = atoi(dashTok[1]);
        int val     = atoi(value.data());
        if (val >= l_limit && val <= u_limit) {
          return true;
        }
      }
    }
    return false;
  }

  bool
  recordIPCheck(std::string_view pattern, std::string_view value)
  {
    //  regex_t regex;
    //  int result;
    bool check;
    std::string_view range_pattern = R"(\[[0-9]+\-[0-9]+\]\\\.\[[0-9]+\-[0-9]+\]\\\.\[[0-9]+\-[0-9]+\]\\\.\[[0-9]+\-[0-9]+\])";
    std::string_view ip_pattern    = "[0-9]*[0-9]*[0-9].[0-9]*[0-9]*[0-9].[0-9]*[0-9]*[0-9].[0-9]*[0-9]*[0-9]";

    Tokenizer dotTok1(".");
    Tokenizer dotTok2(".");

    check = true;
    if (recordRegexCheck(range_pattern, pattern) && recordRegexCheck(ip_pattern, value)) {
      if (dotTok1.Initialize(const_cast<char *>(pattern.data()), COPY_TOKS) == 4 &&
          dotTok2.Initialize(const_cast<char *>(value.data()), COPY_TOKS) == 4) {
        for (int i = 0; i < 4 && check; i++) {
          if (!recordRangeCheck(dotTok1[i], dotTok2[i])) {
            check = false;
          }
        }
        if (check) {
          return true;
        }
      }
    } else if (strcmp(value, "") == 0) {
      return true;
    }
    return false;
  }
} // namespace

bool
recordValidityCheck(std::string_view value, RecCheckT checkType, std::string_view pattern)
{
  switch (checkType) {
  case RECC_STR:
    if (recordRegexCheck(pattern, value)) {
      return true;
    }
    break;
  case RECC_INT:
    if (recordRangeCheck(pattern, value)) {
      return true;
    }
    break;
  case RECC_IP:
    if (recordIPCheck(pattern, value)) {
      return true;
    }
    break;
  case RECC_NULL:
    // skip checking
    return true;
  default:
    // unknown RecordCheckType...
    ;
  }

  return false;
}

} // namespace rpc::handlers::records::utils