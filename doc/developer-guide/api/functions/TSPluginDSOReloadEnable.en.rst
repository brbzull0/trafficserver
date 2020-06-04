.. Licensed to the Apache Software Foundation (ASF) under one
   or more contributor license agreements.  See the NOTICE file
   distributed with this work for additional information
   regarding copyright ownership.  The ASF licenses this file
   to you under the Apache License, Version 2.0 (the
   "License"); you may not use this file except in compliance
   with the License.  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

.. include:: ../../../common.defs

.. default-domain:: c

TSPluginDSOReloadEnable
*************************

Optout option to not take part in the remap dynamic reload preocess (remap.config)

Synopsis
========

.. code-block:: cpp

    #include <ts/ts.h>

.. function:: TSReturnCode TSPluginDSOReloadEnable(int)

Description
===========

:func:`TSPluginDSOReloadEnable` Set a flag to disable(`false`)/enable(`true`) a particular mixed(Global/Remap) plugin to take 
part in the dynamic reload mechanism. This function must be called from inside the :func:`TSPluginInit`.

This function should be used to block the dynamic reload feature for remap plugins (remap.config) 
on a particular plugin. See :ts:cv:`proxy.config.plugin.dynamic_reload_mode`.


See Also
========

:manpage:`TSAPI(3ts)`
