'''
Test rate_limit plugin: SNI metrics are named <prefix>.sni.<tag>.

With the bug, parseYaml() passed (prefix, tag) into
initializeMetrics(type, tag, prefix), so the two halves came out reversed for
every SNI limiter with a `metrics` node, including when only one of the two
keys was set, because the defaults were swapped along with them.
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

Test.Summary = __doc__

ts = Test.MakeATSProcess("ts")

# The metrics are registered by SniSelector::startup() when the plugin loads, so
# no traffic is needed to observe their names.
rate_limit_yaml = os.path.join(ts.Variables.CONFIGDIR, 'rate_limit.yaml')
ts.Disk.File(
    rate_limit_yaml, typename="ats:config").AddLines(
        [
            'selector:',
            '  - sni: both.example.com',
            '    limit: 100',
            '    metrics:',
            '      prefix: pfx-both',
            '      tag: tag-both',
            '  - sni: tagonly.example.com',
            '    limit: 100',
            '    metrics:',
            '      tag: tag-only',
            '  - sni: prefixonly.example.com',
            '    limit: 100',
            '    metrics:',
            '      prefix: pfx-only',
            '  - sni: nometrics.example.com',
            '    limit: 100',
            '',
        ])

ts.Disk.records_config.update(
    {
        'proxy.config.diags.debug.enabled': 1,
        'proxy.config.diags.debug.tags': 'rate_limit',
        'proxy.config.url_remap.remap_required': 0,
    })

ts.Disk.plugin_config.AddLine(f'rate_limit.so {rate_limit_yaml}')

# Every name below, correct or swapped, contains ".sni.", so a single query
# covers all of the cases.
tr = Test.AddTestRun("SNI limiter metrics are named <prefix>.sni.<tag>")
tr.Processes.Default.Env = ts.Env
tr.Processes.Default.StartBefore(ts)
tr.Processes.Default.Command = "traffic_ctl metric match '\\.sni\\.'"
tr.Processes.Default.ReturnCode = 0

# Both keys set.
tr.Processes.Default.Streams.stdout = Testers.ContainsExpression(
    r'pfx-both\.sni\.tag-both\.queued', 'An explicit prefix and tag must be used in that order.')
tr.Processes.Default.Streams.stdout += Testers.ExcludesExpression(
    r'tag-both\.sni\.pfx-both', 'The prefix and tag must not be transposed.')

# Only the tag set: the prefix falls back to the plugin default.
tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    r'plugin\.rate_limiter\.sni\.tag-only\.queued', 'A tag without a prefix must keep the default prefix.')
tr.Processes.Default.Streams.stdout += Testers.ExcludesExpression(
    r'tag-only\.sni\.plugin\.rate_limiter', 'The default prefix must not be used as the tag.')

# Only the prefix set: the tag falls back to the selector name.
tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
    r'pfx-only\.sni\.prefixonly\.example\.com\.queued', 'A prefix without a tag must fall back to the selector name.')
tr.Processes.Default.Streams.stdout += Testers.ExcludesExpression(
    r'prefixonly\.example\.com\.sni\.pfx-only', 'The selector name must not be used as the prefix.')

# All four suffixes are named off the same base.
for suffix in ['queued', 'rejected', 'expired', 'resumed']:
    tr.Processes.Default.Streams.stdout += Testers.ContainsExpression(
        rf'pfx-both\.sni\.tag-both\.{suffix}', f'The {suffix} metric must share the same base name.')

# A selector without a `metrics` node registers no metrics at all.
tr.Processes.Default.Streams.stdout += Testers.ExcludesExpression(
    r'nometrics\.example\.com', 'A selector without a metrics node must not register metrics.')

tr.StillRunningAfter = ts
