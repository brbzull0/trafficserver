'''
Verify how TSMgmtUpdateRegister callbacks interact with the reload task tree's
file gate and config keys.
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

Test.Summary = 'TSMgmtUpdateRegister file gate, missing gated file, and a name that matches a core config key'
Test.ContinueOnFail = True

ts = Test.MakeATSProcess('ts')
ts.Disk.records_config.update(
    {
        'proxy.config.diags.debug.enabled': 1,
        'proxy.config.diags.debug.tags': 'config.reload|mgmt_update_plugin_test',
    })
ts.Disk.MakeConfigFile('gap_gated.conf').AddLine('mode: accept')

CALLBACKS_TASK = 'TSMgmtUpdateRegister callbacks'
coll_marker = os.path.join(ts.Variables.LOGDIR, 'gap_coll.marker')
gated_marker = os.path.join(ts.Variables.LOGDIR, 'gap_gated.marker')
plugin_so = os.path.join(Test.Variables.AtsBuildGoldTestsDir, 'jsonrpc', 'plugins', '.libs', 'mgmt_update_plugin_test.so')
gated_conf = os.path.join(ts.Variables.CONFIGDIR, 'gap_gated.conf')

# A 2-arg registration whose name is a core config key, slow, so a reload that
# does not wait for it is visible.
Test.PrepareTestPlugin(plugin_so, ts, f'--name=ip_allow --delay=3000 --marker={coll_marker}')
Test.PrepareTestPlugin(plugin_so, ts, f'--name=gap_gated --file=gap_gated.conf --marker={gated_marker}')
# An empty registration name must still be counted and shown.
Test.PrepareTestPlugin(plugin_so, ts, '--name=')


def status_check(token, *testers):
    tr = Test.AddTestRun(f'Check task tree of {token}')
    tr.Processes.Default.Command = f'traffic_ctl config status -t {token}'
    tr.Processes.Default.Env = ts.Env
    tr.Processes.Default.ReturnCode = 0
    tr.Processes.Default.Streams.stdout = All(*testers)
    tr.StillRunningAfter = ts
    return tr


# G1. Plugin named like a core config key, with that core file touched. The
#     reload must still wait for the callback.
tr = Test.AddTestRun('Start ATS and touch ip_allow.yaml')
tr.Processes.Default.StartBefore(ts)
tr.Processes.Default.Command = f'touch {ts.Disk.ip_allow_yaml.AbsRunTimePath}'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

Test.AddConfigReload(
    ts, token='gap-1', expect='success', delay_start=1, expect_tasks={
        CALLBACKS_TASK: 'success',
        'ip_allow.yaml': 'success'
    })

tr = Test.AddTestRun('The colliding callback returned before the reload was reported complete')
tr.Processes.Default.Command = f'cat {coll_marker}'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Streams.stdout = Testers.ContainsExpression(r'^returned 1$', 'callback returned')
tr.StillRunningAfter = ts

# G2. Change the gated file: dispatched once.
tr = Test.AddTestRun('Change the gated file')
tr.DelayStart = 1
tr.Processes.Default.Command = f'echo "mode: changed" > {gated_conf}'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

Test.AddConfigReload(ts, token='gap-2', expect='success', expect_tasks={CALLBACKS_TASK: 'success'})
status_check('gap-2', Testers.ContainsExpression(r"Plugin 'gap_gated' was notified", 'gated callback dispatched'))

# G3. Reload again without changing the gated file: it must not be dispatched again.
Test.AddConfigReload(ts, token='gap-3', expect='success', delay_start=1)
status_check(
    'gap-3',
    Testers.ContainsExpression(
        r"Notifying 2 plugin\(s\) registered with TSMgmtUpdateRegister: ('ip_allow', ''|'', 'ip_allow')$",
        'only the 2-arg callbacks are due, each quoted and separated'),
    Testers.ExcludesExpression(r"Plugin 'gap_gated'", 'gate consumed the mtime already'),
)
tr = Test.AddTestRun('The gated callback ran exactly once')
tr.Processes.Default.Command = f'cat {gated_marker}'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Streams.stdout = All(
    Testers.ContainsExpression(r'^returned 1$', 'first call recorded'),
    Testers.ExcludesExpression(r'^returned 2', 'no second call'))
tr.StillRunningAfter = ts

# G4. Remove the gated file: the stat fails, the callback is not dispatched.
tr = Test.AddTestRun('Remove the gated file')
tr.Processes.Default.Command = f'rm -f {gated_conf}'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts

Test.AddConfigReload(ts, token='gap-4', expect='success', delay_start=1)
status_check(
    'gap-4',
    Testers.ContainsExpression(r'Notifying 2 plugin\(s\)', 'only the 2-arg callbacks are due'),
    Testers.ContainsExpression(r"\[Warn\]  Plugin 'gap_gated' was not notified: cannot stat", 'the skipped plugin is named'),
    Testers.ExcludesExpression(
        r"Plugin 'gap_gated' was notified", 'a gated callback whose file cannot be stat-ed is not dispatched'),
)

ts.Disk.diags_log.Content = Testers.ContainsExpression(r'Failed to stat .*gap_gated\.conf', 'G4 stat failure logged')
