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

Test.Summary = '''
Test a basic remap of a http connection
'''

Test.ContinueOnFail = True
# Define default ATS
ts = Test.MakeATSProcess("ts")
server = Test.MakeOriginServer("server", lookup_key="{%Host}{PATH}")

Test.testName = "Test conf_remap using a yaml file."


request_header2 = {"headers": "GET /test HTTP/1.1\r\nHost: www.testexample.com\r\n\r\n",
                   "timestamp": "1469733493.993", "body": ""}
# expected response from the origin server
response_header2 = {"headers": "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n",
                    "timestamp": "1469733493.993", "body": ""}

# add response to the server dictionary
server.addResponse("sessionfile.log", request_header2, response_header2)
ts.Disk.records_config.update(
    '''
  diags:
    debug:
      enabled: 1
      tags: conf_remap
  dns:
    resolv_conf: NULL
  http:
    referer_filter: 1
  url_remap:
    pristine_host_hdr: 0 # make sure is 0

'''
)

ts.Disk.MakeConfigFile("testexample_remap.yaml").update(
    '''
    ts:
      url_remap:
        pristine_host_hdr: 1
    '''
)

ts.Disk.remap_config.AddLine(
    'map http://www.testexample.com http://127.0.0.1:{0} @plugin=conf_remap.so @pparam=testexample_remap.yaml'.format(
        server.Variables.Port))


# microserver lookup test
tr = Test.AddTestRun()
tr.Processes.Default.Command = 'curl --proxy 127.0.0.1:{0} "http://www.testexample.com/test" -H "Host: www.testexample.com" --verbose'.format(
    ts.Variables.port)
tr.Processes.Default.ReturnCode = 0
tr.Processes.Default.StartBefore(Test.Processes.ts)
tr.Processes.Default.StartBefore(server)
tr.Processes.Default.Streams.stderr = "gold/lookupTest.gold"
tr.StillRunningAfter = server

ts.Disk.traffic_out.Content = Testers.ContainsExpression(
    "Setting config id 0 to 1",
    "It should fail when legacy config is found.")
