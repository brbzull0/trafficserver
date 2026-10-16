/** @file

  Internal SDK stuff

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

#include "api/InkAPIInternal.h"
#include "mgmt/config/ReloadCoordinator.h"

#include <atomic>
#include <utility>
#include <vector>

#include "swoc/bwf_base.h"

namespace
{
DbgCtl dbg_ctl_config{"config.reload"};
} // namespace

/// Tracks the TSMgmtUpdateRegister callbacks dispatched by one reload as a single core subtask.
///
/// These callbacks cannot report whether the plugin accepted its configuration, so the subtask only
/// claims what core knows: every callback was delivered and has returned. Until then the main reload
/// task stays in progress, unless a core subtask fails or the reload times out. Each plugin gets a
/// warning instead of a success of its own.
struct ConfigUpdateCallback::Batch {
  Batch(ConfigContext c, std::size_t n) : ctx(std::move(c)), pending(n) {}

  void
  returned(std::string const &name)
  {
    std::string buf;

    // Log before counting down, so the last warning is in place before the subtask completes.
    ctx.log(DL_Warning, swoc::bwprint(buf,
                                      "Plugin '{}' was notified, but TSMgmtUpdateRegister callbacks cannot report whether the "
                                      "plugin accepted its configuration; check diags.log",
                                      name));
    if (pending.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      ctx.complete("All TSMgmtUpdateRegister callbacks returned");
    }
  }

  ConfigContext            ctx;
  std::atomic<std::size_t> pending;
};

int
ConfigUpdateCallback::event_handler(int, void *)
{
  if (m_cont->mutex) {
    MUTEX_TRY_LOCK(trylock, m_cont->mutex, this_ethread());
    if (!trylock.is_locked()) {
      eventProcessor.schedule_in(this, HRTIME_MSECONDS(10), ET_TASK);
      return 0;
    }
    m_cont->handleEvent(TS_EVENT_MGMT_UPDATE, nullptr);
  } else {
    m_cont->handleEvent(TS_EVENT_MGMT_UPDATE, nullptr);
  }

  m_batch->returned(m_name);
  delete this;

  return 0;
}

ConfigUpdateCbTable::ConfigUpdateCbTable() {}

ConfigUpdateCbTable::~ConfigUpdateCbTable() {}

void
ConfigUpdateCbTable::insert(INKContInternal *contp, const char *name, const char *file_name)
{
  ink_assert(contp != nullptr);
  ink_assert(name != nullptr);

  if (nullptr != file_name) {
    swoc::file::path file_path{file_name};
    std::error_code  ec;
    auto             timestamp = swoc::file::last_write_time(file_path, ec);

    if (!ec) {
      cb_table.emplace(name, std::make_tuple(contp, file_path, timestamp));
    } else {
      Error("Failed to stat %s: %s", file_path.c_str(), ec.message().c_str());
    }
  } else {
    cb_table.emplace(name, std::make_tuple(contp, swoc::file::path{}, swoc::file::file_time_type{}));
  }
}

void
ConfigUpdateCbTable::invoke()
{
  std::vector<std::pair<std::string, INKContInternal *>> due;
  std::vector<std::string>                               not_notified;
  std::string                                            buf;

  for (auto &&it : cb_table) {
    auto &[contp, file_path, timestamp] = it.second;

    if (!file_path.empty()) {
      std::error_code ec;
      auto            newtime = swoc::file::last_write_time(file_path, ec);

      if (!ec) {
        if (newtime > timestamp) {
          timestamp = newtime;
          due.emplace_back(it.first, contp);
        }
      } else {
        Error("Failed to stat %s: %s", file_path.c_str(), ec.message().c_str());
        not_notified.emplace_back(
          swoc::bwprint(buf, "Plugin '{}' was not notified: cannot stat {}: {}", it.first, file_path.c_str(), ec.message()));
      }
    } else {
      due.emplace_back(it.first, contp);
    }
  }

  if (due.empty() && not_notified.empty()) {
    return;
  }

  // Create the subtask before scheduling anything, so the main reload task cannot reach a terminal
  // state while these callbacks are still pending. No config key: it must never dedup against a
  // registered config of the same name.
  auto ctx = ReloadCoordinator::Get_Instance().create_config_context("", "TSMgmtUpdateRegister callbacks");

  for (auto const &msg : not_notified) {
    ctx.log(DL_Warning, msg);
  }

  if (due.empty()) {
    ctx.complete("No plugin was notified");
    return;
  }

  std::string names;
  for (auto const &[name, contp] : due) {
    if (!names.empty()) {
      names += ", ";
    }
    names += "'" + name + "'";
  }

  ctx.in_progress("Notifying {} plugin(s) registered with TSMgmtUpdateRegister: {}", due.size(), names);
  Dbg(dbg_ctl_config, "Dispatching %zu TSMgmtUpdateRegister callback(s): %s", due.size(), names.c_str());

  auto batch = std::make_shared<ConfigUpdateCallback::Batch>(ctx, due.size());

  for (auto &[name, contp] : due) {
    eventProcessor.schedule_imm(new ConfigUpdateCallback(contp, std::move(name), batch), ET_TASK);
  }
}
