'''
Verify that a config reload waits for TSMgmtUpdateRegister callbacks and does
not report them as successful plugin reloads.

These callbacks cannot report whether the plugin accepted its configuration.
The reload task tree must therefore not finish before they have returned, and
must say that their outcome is unknown rather than show a success per plugin.
'''
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import os

Test.Summary = 'Config reload tracks TSMgmtUpdateRegister callbacks without claiming their outcome'
Test.ContinueOnFail = True

ts = Test.MakeATSProcess('ts')
ts.Disk.records_config.update(
    {
        'proxy.config.diags.debug.enabled': 1,
        'proxy.config.diags.debug.tags': 'config.reload|mgmt_update_plugin_test',
    })
ts.Disk.ip_allow_yaml.AddLines([
    'ip_allow:',
    '- apply: in',
    '  ip_addrs: 0/0',
    '  action: allow',
    '  methods: ALL',
])
ts.Disk.MakeConfigFile('mgmt_gated.conf').AddLine('mode: accept')

CALLBACKS_TASK = 'TSMgmtUpdateRegister callbacks'
SLOW_DELAY_MS = 6000
slow_marker = os.path.join(ts.Variables.LOGDIR, 'mgmt_slow.marker')
fast_marker = os.path.join(ts.Variables.LOGDIR, 'mgmt_fast.marker')
gated_marker = os.path.join(ts.Variables.LOGDIR, 'mgmt_gated.marker')
plugin_so = os.path.join(Test.Variables.AtsBuildGoldTestsDir, 'jsonrpc', 'plugins', '.libs', 'mgmt_update_plugin_test.so')

# Two registrations without a file fire on every file-source reload: one slow
# (with a mutex), one fast (no mutex). The third is registered with a file, so it
# fires only when that file's mtime moves.
Test.PrepareTestPlugin(plugin_so, ts, f'--name=mgmt_slow --delay={SLOW_DELAY_MS} --marker={slow_marker}')
Test.PrepareTestPlugin(plugin_so, ts, f'--name=mgmt_fast --null-mutex --delay=500 --marker={fast_marker}')
Test.PrepareTestPlugin(plugin_so, ts, f'--name=mgmt_gated --file=mgmt_gated.conf --marker={gated_marker}')

# A second server whose only registration is file-gated and never due. Its
# reloads must look as they did before these callbacks were tracked.
ts_idle = Test.MakeATSProcess('ts_idle')
ts_idle.Disk.MakeConfigFile('mgmt_gated.conf').AddLine('mode: accept')
Test.PrepareTestPlugin(plugin_so, ts_idle, '--name=mgmt_idle --file=mgmt_gated.conf')
ts_idle.Disk.diags_log.Content = Testers.ContainsExpression(r'ERROR: Failed to stat .*mgmt_gated\.conf', 'Step 0 removes the file')
ts_idle.Disk.diags_log.Content += Testers.ExcludesExpression(r'ERROR: (?!Failed to stat)', 'No other errors')
ts_idle.Disk.diags_log.Content += Testers.ExcludesExpression('FATAL:', 'No fatal errors')

# Step 5 makes ip_allow.yaml fail on purpose, so the only ERROR allowed is that one.
ts.Disk.diags_log.Content = Testers.ContainsExpression(
    r'\[mgmt_gated\] rejecting the new configuration', 'The gated plugin should reject the second config')
ts.Disk.diags_log.Content += Testers.ContainsExpression(r'ERROR: ip_allow\.yaml failed to load', 'Step 5 should fail ip_allow')
ts.Disk.diags_log.Content += Testers.ExcludesExpression(r'ERROR: (?!ip_allow\.yaml failed to load)', 'No other errors')
ts.Disk.diags_log.Content += Testers.ExcludesExpression('FATAL:', 'No fatal errors')
ts.Disk.diags_log.Content += Testers.ExcludesExpression('Unrecognized configuration value', 'No unrecognized config values')

touch_ip_allow = f'touch {ts.Disk.ip_allow_yaml.AbsRunTimePath}'


def status_check(token, *testers, delay=None):
    tr = Test.AddTestRun(f'Check task tree of {token}')
    if delay is not None:
        tr.DelayStart = delay
    tr.Processes.Default.Command = f'traffic_ctl config status -t {token}'
    tr.Processes.Default.Env = ts.Env
    tr.Processes.Default.ReturnCode = 0
    tr.Processes.Default.Streams.stdout = All(*testers)
    tr.StillRunningAfter = ts
    return tr


def marker_check(marker, expression, description):
    tr = Test.AddTestRun(description)
    tr.Processes.Default.Command = f'cat {marker}'
    tr.Processes.Default.ReturnCode = 0
    tr.Processes.Default.Streams.stdout = Testers.ContainsExpression(expression, description)
    tr.StillRunningAfter = ts
    return tr


# ============================================================================
# 0. No callback is due: no callbacks subtask, nothing to wait for.
# ============================================================================
tr = Test.AddTestRun('Start the idle server')
tr.Processes.Default.StartBefore(ts_idle)
tr.Processes.Default.Command = 'true'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts_idle

Test.AddConfigReload(ts_idle, token='idle-1', expect='success', expect_absent_tasks=[CALLBACKS_TASK])

# Its file goes away: nothing can be notified, but the tree says which plugin was skipped.
tr = Test.AddTestRun('Remove the idle server gated file')
tr.Processes.Default.Command = f'rm {ts_idle.Variables.CONFIGDIR}/mgmt_gated.conf'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts_idle

Test.AddConfigReload(ts_idle, token='idle-2', expect='success', expect_tasks={CALLBACKS_TASK: 'success'})

tr = Test.AddTestRun('Check task tree of idle-2')
tr.Processes.Default.Command = 'traffic_ctl config status -t idle-2'
tr.Processes.Default.Env = ts_idle.Env
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Streams.stdout = All(
    Testers.ContainsExpression(
        r"\[Warn\]  Plugin 'mgmt_idle' was not notified: cannot stat .*mgmt_gated\.conf", 'The skipped plugin is named'),
    Testers.ContainsExpression(r'No plugin was notified', 'The subtask completes without dispatching anything'),
    Testers.ExcludesExpression(r'Notifying', 'Nothing is dispatched'),
)
tr.StillRunningAfter = ts_idle

# ============================================================================
# 1. The reload must not be reported complete before the callbacks return.
#    A core file is touched so that a core subtask finishes right away; without
#    tracking, the main task would read success while mgmt_slow still sleeps.
# ============================================================================
tr = Test.AddTestRun('Start ATS and touch a core config file')
tr.Processes.Default.StartBefore(ts)
tr.Processes.Default.Command = touch_ip_allow
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

Test.AddConfigReload(
    ts,
    token='mgmt-1',
    expect='success',
    delay_start=1,
    expect_tasks={CALLBACKS_TASK: 'success'},
    expect_absent_tasks=['mgmt_slow', 'mgmt_fast', 'mgmt_gated'])

marker_check(slow_marker, r'^returned 1$', 'The slow callback returned before the reload was reported complete')
marker_check(fast_marker, r'^returned 1$', 'The callback without a mutex ran')

status_check(
    'mgmt-1',
    Testers.ContainsExpression(r'Reload \[success\]', 'Reload should end in success'),
    Testers.ContainsExpression(CALLBACKS_TASK, 'The callbacks should have their own subtask'),
    Testers.ContainsExpression(r'Notifying 2 plugin\(s\)', 'Only the two due callbacks should be dispatched'),
    Testers.ContainsExpression(r"\[Warn\]  Plugin 'mgmt_slow' was notified", 'mgmt_slow outcome should be disclaimed'),
    Testers.ContainsExpression(r"\[Warn\]  Plugin 'mgmt_fast' was notified", 'mgmt_fast outcome should be disclaimed'),
    Testers.ContainsExpression(r'All TSMgmtUpdateRegister callbacks returned', 'Subtask should complete after the last return'),
    Testers.ExcludesExpression(r"Plugin 'mgmt_gated'", 'A file-gated callback whose file did not change is not dispatched'),
    Testers.ExcludesExpression(r'\[plugin: mgmt_', 'No per-plugin success row for a callback that cannot report'),
)

# ============================================================================
# 2. A file-gated callback that rejects its config. Core cannot know about the
#    rejection, so the reload still succeeds, but the tree says it cannot vouch
#    for the plugin instead of claiming it loaded. The TS_EVENT_ERROR return
#    value must not change anything.
# ============================================================================
tr = Test.AddTestRun('Write a config that the gated plugin rejects')
tr.DelayStart = 1
tr.Processes.Default.Command = f'echo reject > {ts.Variables.CONFIGDIR}/mgmt_gated.conf'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

Test.AddConfigReload(ts, token='mgmt-2', expect='success', expect_tasks={CALLBACKS_TASK: 'success'})

marker_check(gated_marker, r'^returned 1 rejected$', 'The gated callback ran and rejected its config')

status_check(
    'mgmt-2',
    Testers.ContainsExpression(r'Notifying 3 plugin\(s\)', 'The gated callback is due once its file changed'),
    Testers.ContainsExpression(
        r"\[Warn\]  Plugin 'mgmt_gated' was notified, but TSMgmtUpdateRegister callbacks cannot report whether the plugin "
        r"accepted its configuration; check diags\.log$", 'mgmt_gated outcome should be disclaimed'),
    Testers.ContainsExpression(r"\[Warn\]  Plugin 'mgmt_slow' was notified", 'mgmt_slow outcome should be disclaimed'),
    Testers.ExcludesExpression(r'\[plugin: mgmt_', 'No per-plugin success row for a callback that cannot report'),
)

# ============================================================================
# 3. While a callback is still running, the reload is in progress, so another
#    reload is refused instead of starting on top of it. Every earlier callback
#    must have returned first: a reload queued behind a sleeping callback on a
#    busy ET_TASK thread would look in progress for an unrelated reason.
# ============================================================================
tr = Test.AddTestRun('Start a reload without monitoring')
tr.DelayStart = SLOW_DELAY_MS / 1000 + 1
tr.Processes.Default.Command = 'traffic_ctl config reload -t mgmt-3'
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

tr = Test.AddTestRun('A second reload is refused while callbacks are pending')
tr.DelayStart = 2
tr.Processes.Default.Command = 'traffic_ctl config reload -t mgmt-4'
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.ReturnCode = 75
tr.Processes.Default.Streams.stdout = Testers.ContainsExpression(
    r'Reload in progress \[mgmt-3\]', 'The pending reload should be reported')
tr.StillRunningAfter = ts

status_check(
    'mgmt-3',
    Testers.ContainsExpression(r'Reload \[in_progress\]', 'The reload is still in progress'),
    Testers.ContainsExpression(r' 1 in-progress ', 'The callbacks subtask is in progress, not merely created'),
)

status_check(
    'mgmt-3',
    Testers.ContainsExpression(r'Reload \[success\]', 'The first reload completes once the callbacks return'),
    Testers.ContainsExpression(r'All TSMgmtUpdateRegister callbacks returned', 'Subtask should complete after the last return'),
    delay=SLOW_DELAY_MS / 1000 + 2)

# ============================================================================
# 4. --force while callbacks are pending. The superseded reload keeps its own
#    subtask and stays timed out after its callback returns; the forced one
#    waits for its own dispatch.
# ============================================================================
tr = Test.AddTestRun('Start a reload that will be superseded')
tr.DelayStart = 1
tr.Processes.Default.Command = 'traffic_ctl config reload -t mgmt-5'
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

Test.AddConfigReload(
    ts, token='mgmt-6', expect='success', force=True, timeout='60s', delay_start=1, expect_tasks={CALLBACKS_TASK: 'success'})

marker_check(slow_marker, r'^returned 5$', 'The forced reload waited for the second slow callback')

status_check(
    'mgmt-6',
    Testers.ContainsExpression(r'Reload \[success\]', 'The forced reload should end in success'),
    Testers.ContainsExpression(r"\[Warn\]  Plugin 'mgmt_slow' was notified", 'mgmt_slow outcome should be disclaimed'),
)

status_check(
    'mgmt-5',
    Testers.ContainsExpression(CALLBACKS_TASK, 'The superseded reload keeps its own callbacks subtask'),
    Testers.ContainsExpression(r'Reload \[timeout\]', 'The superseded reload stays timed out after its callback returns'),
)

# ============================================================================
# 5. A core subtask fails while a callback is still pending. The main
#    task fails right away, and must stay failed once the callbacks subtask
#    completes after it. As in step 3, earlier callbacks must have returned.
# ============================================================================
tr = Test.AddTestRun('Write an ip_allow.yaml that fails to load')
tr.DelayStart = SLOW_DELAY_MS / 1000 + 1
tr.Processes.Default.Command = (
    f"printf 'ip_allow:\\n  - apply: invalid_value\\n    action: not_a_valid_action\\n' > {ts.Disk.ip_allow_yaml.AbsRunTimePath}")
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

Test.AddConfigReload(ts, token='mgmt-7', expect='fail', expect_tasks={'ip_allow.yaml': 'fail'})

status_check(
    'mgmt-7',
    Testers.ContainsExpression(r'Reload \[fail\]', 'The failed core subtask keeps the reload failed'),
    Testers.ContainsExpression(r'All TSMgmtUpdateRegister callbacks returned', 'The callbacks subtask still completes'),
    delay=SLOW_DELAY_MS / 1000 + 2)
