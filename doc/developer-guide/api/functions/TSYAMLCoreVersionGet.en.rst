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

TSYAMLCoreVersionGet
********************

Get the YAML-CPP library version used in core

Synopsis
========

.. code-block:: cpp

   #include <ts/ts.h>

.. function:: const char *TSYAMLCoreVersionGet(void);
.. function:: TSReturnCode TSYAMLCompatibilityCheck(const char *yamlcpp_lib_version, size_t yamlcpp_lib_len);

Description
===========

As the YAML-CPP version used in core can be configured so sometimes there could be a mismatch between core and plugins, this could be
fine if the plugin is not using any TS API that uses TSYaml objects, but if this is not the case, then this APIs are intended to help
plugins to check which version is used in core and correlate that to the one in the plugin.

:type:`TSYAMLCoreVersionGet` Should be used make sure your a plugin is running the very same version of YAML-CPP as the one being
used in the core.


:type:`TSYAMLCompatibilityCheck` Checks the passed YAML-CPP library version against the used(built) in core.

:arg:`yamlcpp_lib_version` should be a string with the yaml-cpp library version the plugin is using. A null terminated string is
expected.

:arg:`yamlcpp_lib_len` size of the passed string.


Example:

   .. code-block:: cpp

         #include <ts/ts.h>

         std::string my_yaml_version {"0.7.0"};
         if (TSYAMLCompatibilityCheck(my_yaml_version.c_str(), my_yaml_version.size()) != TS_SUCCESS) {
            std::cout << "Error: " << TSYAMLCoreVersionGet() << " is used in core.\n";
         }


Return Values
=============

:c:func:`TSYAMLCoreVersionGet` Returns a NULL-terminated string that contains the representation("0.7.0") of the YAML-CPP version used in
 the ore. This string must not be modified or freed. Example:


:c:func:`TSYAMLCompatibilityCheck` Returns :const:`TS_SUCCESS` if no issues, :const:`TS_ERROR` if the :arg:`yamlcpp_lib_version`
was not set, or the ``yamlcpp`` version does not match with the one used in the core.
