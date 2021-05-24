'''
https://github.com/apache/trafficserver/issues/2742
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
https://github.com/apache/trafficserver/issues/2742
'''


class NumberOfRedirectionsTest:
    def __init__(self, testName, numberOfRedirections):
        self._numberOfRedirections = numberOfRedirections
        self._tr = Test.AddTestRun(testName)
        self.setup_ts(f'ts{self._numberOfRedirections}')
        self.setup_dns()
        self.setup_verifier_servers()
        self.add_config()

    def setup_ts(self, name="ts"):
        self._ts = Test.MakeATSProcess(name, enable_cache=False)

    def setup_verifier_servers(self):
        self._bar = Test.MakeVerifierServerProcess("bar", "replay/2742_bar_replay.yaml")
        self._foo = Test.MakeVerifierServerProcess(
            "foo",
            "replay/2742_foo_replay.yaml",
            context={"bar_http_port": self._bar.Variables.http_port})
        # use another server to use the same request uuid, we could also use
        # header_rewrite to change the header when /kau is requested.
        self._foo2 = Test.MakeVerifierServerProcess("foo2", "replay/2742_foo2_replay.yaml")

    def setup_dns(self):
        self._dns = Test.MakeDNServer(f"dns_{self._numberOfRedirections}")
        self._dns.addRecords(records={"foo.test": ["127.0.0.1"]})
        self._dns.addRecords(records={"bar.test": ["127.0.0.1"]})

    def add_config(self):
        self._ts.Disk.records_config.update({
            'proxy.config.diags.debug.enabled': 1,
            'proxy.config.diags.debug.tags': 'http|dns|redirect|http_redirect',
            'proxy.config.http.number_of_redirections': self._numberOfRedirections,
            'proxy.config.dns.nameservers': f'127.0.0.1:{self._dns.Variables.Port}',
            'proxy.config.dns.resolv_conf': 'NULL',
            'proxy.config.url_remap.remap_required': 0,  # need this so the domain gets a chance to be evaluated through DNS
            'proxy.config.http.redirect.actions': 'self:follow',  # redirects to self are not followed by default
        })
        self._ts.Disk.remap_config.AddLines([
            'map http://foo.test/ping http://foo.test:{0}/ping'.format(self._foo.Variables.http_port),
            'map http://foo.test/kau http://foo.test:{0}/kau'.format(self._foo2.Variables.http_port),
            'map http://bar.test/pong http://bar.test:{0}/pong'.format(self._bar.Variables.http_port),
        ])

    def run(self):
        self._tr.Processes.Default.StartBefore(self._foo)
        self._tr.Processes.Default.StartBefore(self._foo2)
        self._tr.Processes.Default.StartBefore(self._bar)
        self._tr.Processes.Default.StartBefore(self._dns)
        self._tr.Processes.Default.StartBefore(self._ts)
        self._tr.Command = "curl -L -vvv foo.test/ping --proxy 127.0.0.1:{0} -H 'uuid: issue2742'".format(
            self._ts.Variables.port)
        self._tr.Processes.Default.Streams.All = f"gold/2742_{self._numberOfRedirections}.gold"
        self._tr.ReturnCode = 0
        self._tr.StillRunningAfter = self._ts


NumberOfRedirectionsTest("Test number_of_redirections=1", 1).run()
