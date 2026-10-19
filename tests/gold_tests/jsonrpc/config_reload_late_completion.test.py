'''
Verify that a reload marked timeout, by the progress checker or by --force,
stays timeout when a deferred subtask completes or fails after it.
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

Test.Summary = 'A deferred subtask that completes or fails late does not rewrite a timed-out reload'
Test.ContinueOnFail = True

# The deferred test plugin completes or fails its reload 5 s after the handler
# runs (3 s + 2 s). Every server triggers it by touching its config file.
plugin_so = os.path.join(Test.Variables.AtsBuildGoldTestsDir, 'jsonrpc', 'plugins', '.libs', 'cfg_plugin_deferred_test.so')
PLUGIN_CONF = 'cfg_plugin_deferred_test.conf'
LATE_COMPLETE = 'deferred complete after heavy work'
LATE_FAIL = 'deferred fail after heavy work'


def make_server(name, records, plugin_config='mode: success'):
    ts = Test.MakeATSProcess(name)
    ts.Disk.records_config.update(records)
    ts.Disk.MakeConfigFile(PLUGIN_CONF).AddLine(plugin_config)
    Test.PrepareTestPlugin(plugin_so, ts, PLUGIN_CONF)
    return ts


debug = {
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'config.reload|cfg_plugin_deferred_test',
}
short_timeout = {**debug, 'proxy.config.admin.reload.timeout': '1s', 'proxy.config.admin.reload.check_interval': '1s'}

# ts: the progress checker times the reload out long before the plugin completes.
ts = make_server('ts', short_timeout)
# ts2: default timeout; the reload is superseded with --force instead.
ts2 = make_server('ts2', debug)
# ts3: as ts, but the plugin fails late instead of completing.
ts3 = make_server('ts3', short_timeout, 'defer_fail: true')
ts3.Disk.diags_log.Content = Testers.ContainsExpression(
    'ERROR: cfg_plugin_deferred_test: stage 1 - heavy work failed', 'The plugin logs its late failure')


def run(proc, cmd, desc, delay=None, testers=()):
    tr = Test.AddTestRun(desc)
    if delay is not None:
        tr.DelayStart = delay
    tr.Processes.Default.Command = cmd
    tr.Processes.Default.Env = proc.Env
    tr.Processes.Default.ReturnCode = 0
    if testers:
        tr.Processes.Default.Streams.stdout = All(*testers)
    tr.StillRunningAfter = proc
    return tr


def touch_and_reload(proc, token):
    return f'touch {proc.Variables.CONFIGDIR}/{PLUGIN_CONF} && traffic_ctl config reload -t {token}'


tr = Test.AddTestRun('Start the servers')
tr.Processes.Default.StartBefore(ts)
tr.Processes.Default.StartBefore(ts2)
tr.Processes.Default.StartBefore(ts3)
tr.Processes.Default.Command = 'true'
tr.Processes.Default.ReturnCode = 0
tr.StillRunningAfter = ts
tr.StillRunningAfter = ts2
tr.StillRunningAfter = ts3

# --- Timed out by the progress checker -------------------------------------
run(ts, touch_and_reload(ts, 'to-1'), 'Reload to-1 with a 1 s timeout and a 5 s deferred plugin', delay=1)
run(
    ts,
    'traffic_ctl config status -t to-1',
    'to-1 times out while the plugin is still working',
    delay=3,
    testers=[
        Testers.ContainsExpression(r'Reload \[timeout\]', 'The reload timed out'),
        Testers.ExcludesExpression(LATE_COMPLETE, 'The plugin has not completed yet'),
    ])
run(
    ts,
    'traffic_ctl config status -t to-1',
    'to-1 stays timeout after the plugin completes',
    delay=5,
    testers=[
        Testers.ContainsExpression(LATE_COMPLETE, 'The plugin completed late'),
        Testers.ContainsExpression(r'\b0 in-progress', 'The plugin subtask recorded its completion'),
        Testers.ContainsExpression(r'Reload \[timeout\]', 'The reload is still timeout'),
        Testers.ExcludesExpression(r'Reload \[success\]', 'The late completion did not turn it into success'),
    ])

# --- Superseded by --force ---------------------------------------------------
run(ts2, touch_and_reload(ts2, 'f-1'), 'Reload f-1 with a 5 s deferred plugin')
run(ts2, 'traffic_ctl config reload -t f-2 --force', 'Force f-2 over f-1', delay=1)
run(
    ts2,
    'traffic_ctl config status -t f-1',
    'f-1 is marked superseded while the plugin is still working',
    delay=1,
    testers=[
        Testers.ContainsExpression(r'Reload \[timeout\]', 'f-1 was superseded'),
        Testers.ExcludesExpression(LATE_COMPLETE, 'The plugin has not completed yet'),
    ])
run(
    ts2,
    'traffic_ctl config status -t f-1',
    'f-1 stays timeout after the plugin completes',
    delay=5,
    testers=[
        Testers.ContainsExpression(LATE_COMPLETE, 'The plugin completed late'),
        Testers.ContainsExpression(r'\b0 in-progress', 'The plugin subtask recorded its completion'),
        Testers.ContainsExpression(r'Reload \[timeout\]', 'f-1 is still timeout'),
        Testers.ExcludesExpression(r'Reload \[success\]', 'The late completion did not turn it into success'),
    ])

# --- Timed out by the progress checker, then the plugin fails ---------------
run(ts3, touch_and_reload(ts3, 'tf-1'), 'Reload tf-1 with a 1 s timeout and a 5 s deferred failing plugin')
run(
    ts3,
    'traffic_ctl config status -t tf-1',
    'tf-1 times out while the plugin is still working',
    delay=3,
    testers=[
        Testers.ContainsExpression(r'Reload \[timeout\]', 'The reload timed out'),
        Testers.ExcludesExpression(LATE_FAIL, 'The plugin has not failed yet'),
    ])
run(
    ts3,
    'traffic_ctl config status -t tf-1',
    'tf-1 stays timeout after the plugin fails',
    delay=5,
    testers=[
        Testers.ContainsExpression(LATE_FAIL, 'The plugin failed late'),
        Testers.ContainsExpression(r'\b1 failed', 'The plugin subtask recorded its failure'),
        Testers.ContainsExpression(r'Reload \[timeout\]', 'The reload is still timeout'),
        Testers.ExcludesExpression(r'Reload \[fail\]', 'The late failure did not turn it into fail'),
    ])
