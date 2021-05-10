'''
Specific test for https://github.com/apache/trafficserver/issues/2742
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
Test.Summary = '''
Test redirection with location & number of redirects.
'''


Test.ContinueOnFail = True

ts = Test.MakeATSProcess("ts", enable_cache=False)

foo_serv = Test.MakeOriginServer("foo_server")
bar_serv = Test.MakeOriginServer("bar_server")
kau_serv = Test.MakeOriginServer("kau_server")
dns = Test.MakeDNServer("dns")

dns.addRecords(records={"foo.test": ["127.0.0.1"]})
dns.addRecords(records={"bar.test": ["127.0.0.1"]})
dns.addRecords(records={"kau.test": ["127.0.0.1"]})

ts.Disk.records_config.update({
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'http|dns|redirect|http_redirect',
    'proxy.config.http.redirection_enabled': 1,
    'proxy.config.http.number_of_redirections': 1,
    'proxy.config.dns.nameservers': '127.0.0.1:{0}'.format(dns.Variables.Port),
    'proxy.config.dns.resolv_conf': 'NULL',
    'proxy.config.url_remap.remap_required': 0,
    'proxy.config.http.redirect.actions': 'self:follow',
})

ts.Disk.remap_config.AddLines([
    'map foo.test/ http://foo.test:{0}/'.format(foo_serv.Variables.Port),
    'map bar.test/ http://bar.test:{0}/'.format(bar_serv.Variables.Port),
    'map kau.test/ http://kau.test:{0}/'.format(kau_serv.Variables.Port),
])

'''
  Client -> ATS:GET foo.test/
  ATS -> foo.test: GET foo.test/
  ATS <- foo.test: 302 Redirect to bar.test/

  ATS -> bar.test: GET bar.test/ (the redirect is followed by ATS)
  ATS <- bar.test: 302 Redirect to /kau
  Client <- ATS: 302 Redirect to /kau (the redirect is not followed ATS)
'''

foo_request_header = {
    "headers": "GET / HTTP/1.1\r\nHost: foo.test{0}\r\n\r\n".format(foo_serv.Variables.Port), "timestamp": "5678", "body": ""
}
foo_response_header = {
    "headers": "HTTP/1.1 302 Found\r\nLocation: http://bar.test:{0}/\r\n\r\n".format(bar_serv.Variables.Port), "timestamp": "5678", "body": ""
}
foo_serv.addResponse("sessionfile.log", foo_request_header, foo_response_header)

bar_request_header = {
    "headers": "GET / HTTP/1.1\r\nHost: bar.test:{0}\r\n\r\n".format(bar_serv.Variables.Port), "timestamp": "11", "body": ""
}
bar_response_header = {
    "headers": "HTTP/1.1 302 Found\r\nLocation: http://kau.test:{0}/\r\n\r\n".format(kau_serv.Variables.Port), "timestamp": "5678", "body": ""
}

bar_serv.addResponse("sessionfile.log", bar_request_header, bar_response_header)

kau_request_header = {
    "headers": "GET / HTTP/1.1\r\nHost: kau.test:{0}\r\n\r\n".format(kau_serv.Variables.Port), "timestamp": "11", "body": ""
}
kau_response_header = {
    "headers": "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n", "timestamp": "5678", "body": ""
}
kau_serv.addResponse("sessionfile.log", kau_request_header, kau_response_header)

tr = Test.AddTestRun("Test Redirect Locaction.")
tr.Processes.Default.StartBefore(foo_serv)
tr.Processes.Default.StartBefore(bar_serv)
tr.Processes.Default.StartBefore(kau_serv)
tr.Processes.Default.StartBefore(dns)
tr.Processes.Default.StartBefore(ts)
tr.Command = "curl -L -vvv foo.test/ --proxy 127.0.0.1:{0} ".format(ts.Variables.port)
tr.ReturnCode = 0
tr.StillRunningAfter = ts