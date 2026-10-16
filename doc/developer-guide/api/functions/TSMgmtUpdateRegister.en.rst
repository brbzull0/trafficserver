.. Licensed to the Apache Software Foundation (ASF) under one or more
   contributor license agreements.  See the NOTICE file distributed
   with this work for additional information regarding copyright
   ownership.  The ASF licenses this file to you under the Apache
   License, Version 2.0 (the "License"); you may not use this file
   except in compliance with the License.  You may obtain a copy of
   the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
   implied.  See the License for the specific language governing
   permissions and limitations under the License.

.. include:: ../../../common.defs

.. default-domain:: cpp

TSMgmtUpdateRegister
********************

Synopsis
========

.. code-block:: cpp

    #include <ts/ts.h>

.. function:: void TSMgmtUpdateRegister(TSCont contp, const char *plugin_name, const char *plugin_file_name=nullptr)

Description
===========

Registers :arg:`contp` to receive :enumerator:`TS_EVENT_MGMT_UPDATE` when
:program:`traffic_ctl config reload` re-reads configuration from disk.
:arg:`plugin_name` identifies the registration; a second registration under the
same name is ignored. If :arg:`plugin_file_name` is given, the event is sent only
when that file's modification time has advanced since the last notification;
otherwise it is sent on every file-source reload. If :arg:`plugin_file_name`
cannot be stat'd when the plugin registers (for example, it is missing), the
registration is dropped and an error is logged. Inline (``--data``) reloads do
not send the event.

The callback runs on an ``ET_TASK`` thread, under :arg:`contp`'s mutex if it has
one. Its return value is ignored.

The reload task tree shows these callbacks under a single core subtask,
``TSMgmtUpdateRegister callbacks``, which completes once every notified callback
has returned. Until then the reload stays in progress, unless a core subtask
fails, so a callback that blocks delays the reload's completion, and other
reloads are refused, unless started with ``--force``, until it returns or
:ts:cv:`proxy.config.admin.reload.timeout` expires. The timeout is checked on an
``ET_TASK`` thread, so it can fire late while callbacks occupy every ``ET_TASK``
thread. Because the callback cannot report an outcome, each plugin is listed with
a warning rather than a success of its own. Plugins that need to report whether
their configuration was accepted should use :func:`TSCfgRegister` instead.

See Also
========

:doc:`TSCfgRegister.en`,
:ref:`config-reload-plugin-api`
