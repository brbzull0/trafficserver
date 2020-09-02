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

#include "ConfigUtils.h"
#include "records/P_RecCore.h"
#include "tscore/Tokenizer.h"

namespace rpc::handlers::config::utils
{
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
} // namespace rpc::handlers::config::utils