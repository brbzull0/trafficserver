'''
Verify edge cases of TSMgmtUpdateRegister dispatch in a reload: a batch of
exactly one callback without a mutex, a registration name that looks like a
format string, and a gated file that comes back after a stat failure.
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

Test.Summary = 'Edge cases of TSMgmtUpdateRegister dispatch in a tracked reload'
Test.ContinueOnFail = True

CALLBACKS_TASK = 'TSMgmtUpdateRegister callbacks'
plugin_so = os.path.join(Test.Variables.AtsBuildGoldTestsDir, 'jsonrpc', 'plugins', '.libs', 'mgmt_update_plugin_test.so')

ts = Test.MakeATSProcess('ts')
ts.Disk.records_config.update(
    {
        'proxy.config.diags.debug.enabled': 1,
        'proxy.config.diags.debug.tags': 'config.reload|mgmt_update_plugin_test',
    })
ts.Disk.MakeConfigFile('edge_gated.conf').AddLine('mode: accept')
gated_conf = os.path.join(ts.Variables.CONFIGDIR, 'edge_gated.conf')
one_marker = os.path.join(ts.Variables.LOGDIR, 'edge_one.marker')
gated_marker = os.path.join(ts.Variables.LOGDIR, 'edge_gated.marker')

# The only 2-arg registration: every reload dispatches a batch of exactly one,
# unless the gated file is also due. It has no mutex, and its name looks like a
# bwformat string.
Test.PrepareTestPlugin(plugin_so, ts, f'--name={{}}{{0}}x --null-mutex --delay=3000 --marker={one_marker}')
Test.PrepareTestPlugin(plugin_so, ts, f'--name=edge_gated --file=edge_gated.conf --marker={gated_marker}')
# E3 removes the gated file on purpose, so the only ERROR allowed is its stat failure.
ts.Disk.diags_log.Content = Testers.ContainsExpression(r'ERROR: Failed to stat .*edge_gated\.conf', 'E3 stat failure logged')
ts.Disk.diags_log.Content += Testers.ExcludesExpression(r'ERROR: (?!Failed to stat)', 'No other errors')
ts.Disk.diags_log.Content += Testers.ExcludesExpression('FATAL:', 'No fatal errors')


def run(cmd, desc, delay=None, rc=0, testers=()):
    tr = Test.AddTestRun(desc)
    if delay is not None:
        tr.DelayStart = delay
    tr.Processes.Default.Command = cmd
    tr.Processes.Default.Env = ts.Env
    tr.Processes.Default.ReturnCode = rc
    if testers:
        tr.Processes.Default.Streams.stdout = All(*testers)
    tr.StillRunningAfter = ts
    return tr


tr = Test.AddTestRun('Start ATS')
tr.Processes.Default.StartBefore(ts)
tr.Processes.Default.Command = 'true'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

# E1. A batch of one, without a mutex: the monitored reload must wait for the
#     single callback and end in success.
Test.AddConfigReload(ts, token='e-1', expect='success', delay_start=1, expect_tasks={CALLBACKS_TASK: 'success'})
run(
    f'cat {one_marker}',
    'E1 the single callback returned before the reload was reported complete',
    testers=[Testers.ContainsExpression(r'^returned 1$', 'single callback returned')])
run(
    'traffic_ctl config status -t e-1',
    'E1 task tree',
    testers=[
        Testers.ContainsExpression(r'Reload \[success\]', 'success'),
        Testers.ContainsExpression(
            r"Notifying 1 plugin\(s\) registered with TSMgmtUpdateRegister: '\{\}\{0\}x'$",
            'the format-like name is printed literally'),
        Testers.ContainsExpression(r"Plugin '\{\}\{0\}x' was notified", 'the format-like name is printed literally in the warning'),
        Testers.ContainsExpression(r'All TSMgmtUpdateRegister callbacks returned', 'completed'),
    ])

# E3. Stat failure, then the file comes back: it must be dispatched again.
run(f'rm -f {gated_conf}', 'E3 remove the gated file')
Test.AddConfigReload(ts, token='e-3', expect='success', delay_start=1)
run(
    'traffic_ctl config status -t e-3',
    'E3 task tree names the skipped plugin',
    testers=[
        Testers.ContainsExpression(r"\[Warn\]  Plugin 'edge_gated' was not notified: cannot stat", 'skipped plugin named'),
        Testers.ContainsExpression(r'Notifying 1 plugin\(s\)', 'only the 2-arg callback is due'),
    ])
run(f"printf 'mode: back\\n' > {gated_conf}", 'E3 recreate the gated file', delay=1)
Test.AddConfigReload(ts, token='e-4', expect='success', delay_start=1)
run(
    'traffic_ctl config status -t e-4',
    'E3 recreated gated file is dispatched again',
    testers=[
        Testers.ContainsExpression(r'Notifying 2 plugin\(s\)', 'both callbacks due'),
        Testers.ContainsExpression(r"\[Warn\]  Plugin 'edge_gated' was notified", 'gated plugin notified'),
        Testers.ExcludesExpression(r'was not notified', 'no stale not-notified warning'),
    ])
