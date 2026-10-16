/** @file

  Test plugin for TSMgmtUpdateRegister callbacks during a config reload. The
  callback can block for a while, record each return in a marker file, and,
  when registered with a file, reject a config by keeping its previous one.

  Arguments:
    --name=<name>      registration name passed to TSMgmtUpdateRegister
    --file=<path>      register with a file (only fires when its mtime moves)
    --delay=<ms>       block the callback this long before returning
    --marker=<path>    append "returned <n>" to this file on every return
    --null-mutex       create the continuation without a mutex

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

#include <ts/ts.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#define PLUGIN_NAME "mgmt_update_plugin_test"

namespace
{
DbgCtl dbg_ctl{PLUGIN_NAME};

struct PluginState {
  std::string      name{PLUGIN_NAME};
  std::string      file;
  std::string      marker;
  int              delay_ms{0};
  std::atomic<int> calls{0};
};

int
mgmt_update(TSCont contp, TSEvent event, void * /* edata */)
{
  auto *state = static_cast<PluginState *>(TSContDataGet(contp));
  int   call  = ++state->calls;

  Dbg(dbg_ctl, "[%s] callback %d begin (event %d)", state->name.c_str(), call, static_cast<int>(event));

  if (state->delay_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(state->delay_ms));
  }

  bool rejected = false;

  if (!state->file.empty()) {
    std::ifstream      in(state->file);
    std::ostringstream ss;

    ss << in.rdbuf();
    if (ss.str().find("reject") != std::string::npos) {
      rejected = true;
      TSWarning("[%s] rejecting the new configuration, keeping the previous one", state->name.c_str());
    }
  }

  if (!state->marker.empty()) {
    std::ofstream out(state->marker, std::ios::app);

    out << "returned " << call << (rejected ? " rejected" : "") << '\n';
  }

  Dbg(dbg_ctl, "[%s] callback %d end", state->name.c_str(), call);

  // A rejection returns TS_EVENT_ERROR to show that core does not read the return value.
  return rejected ? TS_EVENT_ERROR : TS_EVENT_NONE;
}

std::string
config_relative(std::string_view path)
{
  if (!path.empty() && path.front() != '/') {
    return std::string(TSConfigDirGet()) + "/" + std::string(path);
  }
  return std::string(path);
}

} // anonymous namespace

void
TSPluginInit(int argc, const char *argv[])
{
  TSPluginRegistrationInfo info;

  info.plugin_name   = PLUGIN_NAME;
  info.vendor_name   = "Apache Software Foundation";
  info.support_email = "dev@trafficserver.apache.org";

  if (TSPluginRegister(&info) != TS_SUCCESS) {
    TSError("[%s] Plugin registration failed", PLUGIN_NAME);
    return;
  }

  auto *state      = new PluginState;
  bool  null_mutex = false;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};

    if (arg.starts_with("--name=")) {
      state->name = arg.substr(7);
    } else if (arg.starts_with("--file=")) {
      state->file = config_relative(arg.substr(7));
    } else if (arg.starts_with("--delay=")) {
      state->delay_ms = std::stoi(std::string(arg.substr(8)));
    } else if (arg.starts_with("--marker=")) {
      state->marker = arg.substr(9);
    } else if (arg == "--null-mutex") {
      null_mutex = true;
    } else {
      TSError("[%s] unknown argument: %s", PLUGIN_NAME, argv[i]);
    }
  }

  TSCont contp = TSContCreate(mgmt_update, null_mutex ? nullptr : TSMutexCreate());

  TSContDataSet(contp, state);
  TSMgmtUpdateRegister(contp, state->name.c_str(), state->file.empty() ? nullptr : state->file.c_str());

  Dbg(dbg_ctl, "[%s] registered (file: %s, delay: %dms, null mutex: %s)", state->name.c_str(),
      state->file.empty() ? "none" : state->file.c_str(), state->delay_ms, null_mutex ? "yes" : "no");
}
