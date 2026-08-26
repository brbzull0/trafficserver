'''
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
import re
from jsonrpc import Notification, Request, Response

Test.Summary = 'Basic records test. Testing the new records.yaml logic and making sure it works as expected.'

ts = Test.MakeATSProcess("ts")

ts.Disk.records_config.update(
    '''
    accept_threads: 1
    cache:
      limits:
        http:
          max_alts: 5
    diags:
      debug:
        enabled: 0
        tags: http|dns
    ''')

# 0 - We want to make sure that the unregistered records are still being detected.
tr = Test.AddTestRun("Test Append value to existing records.yaml")

tr.Processes.Default.Command = 'traffic_ctl config set proxy.config.diags.debug.tags rpc --cold'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.StartBefore(ts)
ts.Disk.records_config.Content = 'gold/records.yaml.cold_test0.gold'

# 1
tr = Test.AddTestRun("Get value from latest added node.")
tr.Processes.Default.Command = 'traffic_ctl config get proxy.config.diags.debug.tags --cold'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env

tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    'proxy.config.diags.debug.tags: rpc', 'Config should show the right tags')

# 2
tr = Test.AddTestRun("Test modify latest yaml document from records.yaml")
tr.Processes.Default.Command = 'traffic_ctl config set proxy.config.diags.debug.tags http -u -c'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
ts.Disk.records_config.Content = 'gold/records.yaml.cold_test2.gold'

# 3
tr = Test.AddTestRun("Get value from latest added node 1.")
tr.Processes.Default.Command = 'traffic_ctl config get proxy.config.diags.debug.tags --cold'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env

tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    'proxy.config.diags.debug.tags: http', 'Config should show the right tags')

# 4
tr = Test.AddTestRun("Append a new field node using a tag")
tr.Processes.Default.Command = 'traffic_ctl config set proxy.config.cache.limits.http.max_alts 1 -t int -c'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
ts.Disk.records_config.Content = 'gold/records.yaml.cold_test4.gold'

# 5
file = os.path.join(ts.Variables.CONFIGDIR, "new_records.yaml")
tr = Test.AddTestRun("Adding a new node(with update flag set) to a non existing file")
tr.Processes.Default.Command = f'traffic_ctl config set proxy.config.cache.limits.http.max_alts 3  -u -c {file}'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
tr.Disk.File(file).Content = 'gold/records.yaml.cold_test5.gold'

# 5
file = os.path.join(ts.Variables.CONFIGDIR, "new_records2.yaml")
tr = Test.AddTestRun("Adding a new node to a non existing file")
tr.Processes.Default.Command = f'traffic_ctl config set proxy.config.cache.limits.http.max_alts 3 -c {file}'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
tr.Disk.File(file).Content = 'gold/records.yaml.cold_test5.gold'

# TS_RECORD_YAML names the file when -c is given without one. The option is still required,
# so the variable alone must not turn a get into a file read.
env_file = os.path.join(ts.Variables.CONFIGDIR, "env_records.yaml")

# 6
tr = Test.AddTestRun("Set a value in the file named by TS_RECORD_YAML")
tr.Processes.Default.Command = f'env TS_RECORD_YAML={env_file} traffic_ctl config set proxy.config.cache.limits.http.max_alts 3 -c'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
tr.Disk.File(env_file).Content = 'gold/records.yaml.cold_test5.gold'
tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    f'Set records.cache.limits.http.max_alts in {env_file} \\(from TS_RECORD_YAML\\)',
    'The output must name the file that was written and where the name came from')

# 7
tr = Test.AddTestRun("Get several values from the file named by TS_RECORD_YAML")
tr.Processes.Default.Command = (
    f'env TS_RECORD_YAML={env_file} traffic_ctl config get '
    'proxy.config.cache.limits.http.max_alts proxy.config.diags.debug.tags -c')
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    f'# {env_file} \\(from TS_RECORD_YAML\\)', 'The output must name the file that was read and where the name came from')
tr.Processes.Default.Streams.stdout += Testers.ExcludesExpression(
    'TS_RECORD_YAML\\)[\\s\\S]*TS_RECORD_YAML\\)',
    'The origin must be reported once per invocation, not once per record',
    reflags=re.M)
tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    'proxy.config.cache.limits.http.max_alts: 3', 'The value must come from the file named by the variable')

# 8
tr = Test.AddTestRun("A file name given on the command line is not reported as coming from the environment")
tr.Processes.Default.Command = f'traffic_ctl config get proxy.config.cache.limits.http.max_alts -c {env_file}'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(f'# {env_file}', 'The output must name the file that was read')
tr.Processes.Default.Streams.stdout += Testers.ExcludesExpression('from TS_RECORD_YAML', 'The name was typed, so nothing to report')

# 9
tr = Test.AddTestRun("Without -c the variable is ignored and the server is queried")
tr.Processes.Default.Command = f'env TS_RECORD_YAML={env_file} traffic_ctl config get proxy.config.cache.limits.http.max_alts'
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    'proxy.config.cache.limits.http.max_alts: 5', 'The running value must be reported, not the one in the file')
tr.Processes.Default.Streams.stdout += Testers.ExcludesExpression(
    f'# {env_file}', 'A value from the server must not be marked as coming from a file')
