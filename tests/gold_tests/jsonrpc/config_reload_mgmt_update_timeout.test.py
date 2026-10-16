'''
Verify that a reload marked timeout, by the progress checker or by --force,
stays timeout when a TSMgmtUpdateRegister callback returns after it.
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

Test.Summary = 'A late TSMgmtUpdateRegister callback does not rewrite a timed-out reload'
Test.ContinueOnFail = True

plugin_so = os.path.join(Test.Variables.AtsBuildGoldTestsDir, 'jsonrpc', 'plugins', '.libs', 'mgmt_update_plugin_test.so')

# ts: progress-checker timeout shorter than the callback.
ts = Test.MakeATSProcess('ts')
ts.Disk.records_config.update(
    {
        'proxy.config.diags.debug.enabled': 1,
        'proxy.config.diags.debug.tags': 'config.reload|mgmt_update_plugin_test',
        'proxy.config.admin.reload.timeout': '2s',
        'proxy.config.admin.reload.check_interval': '1s',
    })
Test.PrepareTestPlugin(plugin_so, ts, '--name=mgmt_slow --delay=8000')

# ts2: default timeout, superseded with --force.
ts2 = Test.MakeATSProcess('ts2')
ts2.Disk.records_config.update(
    {
        'proxy.config.diags.debug.enabled': 1,
        'proxy.config.diags.debug.tags': 'config.reload|mgmt_update_plugin_test',
    })
Test.PrepareTestPlugin(plugin_so, ts2, '--name=mgmt_slow2 --null-mutex --delay=8000')


def run(proc, cmd, desc, delay=None, rc=0, expr=None, excl=None, also=None):
    tr = Test.AddTestRun(desc)
    if delay is not None:
        tr.DelayStart = delay
    tr.Processes.Default.Command = cmd
    tr.Processes.Default.Env = proc.Env
    tr.Processes.Default.ReturnCode = rc
    testers = []
    if expr:
        testers.append(Testers.ContainsExpression(expr, desc))
    if excl:
        testers.append(Testers.ExcludesExpression(excl, desc))
    if also:
        testers.append(Testers.ContainsExpression(also, desc))
    if testers:
        tr.Processes.Default.Streams.stdout = All(*testers)
    tr.StillRunningAfter = proc
    return tr


tr = Test.AddTestRun('Start both servers')
tr.Processes.Default.StartBefore(ts)
tr.Processes.Default.StartBefore(ts2)
tr.Processes.Default.Command = 'true'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts
tr.StillRunningAfter = ts2

# --- Timeout by the progress checker ---------------------------------------
run(ts, 'traffic_ctl config reload -t to-1', 'Reload to-1 with an 8s callback and a 2s timeout')
run(ts, 'traffic_ctl config status -t to-1', 'to-1 timed out while the callback sleeps', delay=5, expr=r'Reload \[timeout\]')
run(
    ts,
    'traffic_ctl config status -t to-1',
    'to-1 stays timeout after the callback returns',
    delay=8,
    expr=r'Reload \[timeout\]',
    excl=r'Reload \[success\]',
    also=r'All TSMgmtUpdateRegister callbacks returned')

# --- Superseded by --force ---------------------------------------------------
run(ts2, 'traffic_ctl config reload -t f-1', 'Reload f-1 with an 8s callback')
run(ts2, 'traffic_ctl config reload -t f-2 --force', 'Force f-2 over f-1', delay=2)
run(ts2, 'traffic_ctl config status -t f-1', 'f-1 marked superseded', delay=1, expr=r'Reload \[timeout\]')
run(
    ts2,
    'traffic_ctl config status -t f-1',
    'f-1 stays timeout after its callback returns',
    delay=9,
    expr=r'Reload \[timeout\]',
    excl=r'Reload \[success\]',
    also=r'All TSMgmtUpdateRegister callbacks returned')
