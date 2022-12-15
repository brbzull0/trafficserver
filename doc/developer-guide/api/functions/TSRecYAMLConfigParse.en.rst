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

TSYAMLRecCfgFieldData
*********************

Get the YAML-CPP library version used in core

Synopsis
========

.. code-block:: cpp

   #include <ts/ts.h>
   #include <ts/apidefs.h>

.. c:type:: TSYAMLRecCfgFieldData

   Config field from a records file.

.. type:: TSReturnCode (*TSYAMLRecNodeHandler)(const TSYAMLRecCfgFieldData *cfg, void *data);

   Record field callback

.. function:: TSReturnCode TSRecYAMLConfigParse(TSYaml node, TSYAMLRecNodeHandler handler, void *data);

   Parse a YAML node following the record structure.


Description
===========

.. type:: TSYAMLRecCfgFieldData

   This is used to interact with the records YAML files. This is passed back to the `TSYAMLRecCfgFieldData`
   when you call `TSRecYAMLConfigParse` this class will hold the record name as well as the YAML node.

.. var:: const char* field_name

   A null-terminated string with the YAML field name. This holds the name of the scalar field. This is mostly to be used for logging.

.. var:: const char* record_name

    A null-terminated string with the record name which was built when parsing the YAML file. Example: `proxy.config.diags.enabled`.

.. var:: TSYaml value_node

   A `YAML::Node` holding the value of the field. From this the value should be extracted. You can use the `YAML::Node` API to deduce
   the type, etc.

.. type:: TSReturnCode (*TSYAMLRecNodeHandler)(const TSYAMLRecCfgFieldData *cfg, void *data);

   This should be used to interact with records file in YAMl format, the only
   point is to be used with record files as each `TSYAMLRecCfgFieldData`` contains
   the record name that will be used as well as the YAML node for the scalar value.

   If you need to parse a records.yaml file and need to handle each node separately then
   this API should be used, an example of this would be the conf_remap plugin.

.. function:: TSReturnCode TSRecYAMLConfigParse(TSYaml node, TSYAMLRecNodeHandler handler, void *data);

   Parse a YAML node following the record structure. On every scalar node the `handler` callback will be
   invoked with the appropriate parsed fields.


Example:

   .. code-block:: yaml

      ts:
        plugin_x:
          field_1: 1
          data:
            data_field_1: XYZ


   `TSRecYAMLConfigParse` will parse the node and will detect once it reached `field_1` and `data_field_1` will invoke the handler, the
   handler will hold a `TSYAMLRecCfgFieldData` pointer to the detected field data.


   A coding example using the above yaml file would look like:


   .. code-block:: cpp

      TSReturnCode
      field_handler(const TSYAMLRecCfgFieldData *cfg, void *data/*optional*/)
      {
         YAML::Node value = *reinterpret_cast<YAML::Node *>(cfg->value_node);

         /*
          Using the cfg* we can now if we were parsing a legacy record field.

          This function will be called twice:

          1:
            cfg->field_name = field_1
            cfg->record_name = ts.plugin_x.field_1

          2:
            cfg->field_name = data_field_1
            cfg->record_name = ts.plugin_x.data.data_field_1


          If we need to check the type, we either get the type from the YAML::Node::Tag if set, or from @c TSHttpTxnConfigFind
         */

         return TS_SUCCESS;
      }

      void my_plugin_function_to_parse_a_records_like_yaml_file(const char* path) {
         YAML::Node root = YAML::LoadFile(path);

         auto ret = TSRecYAMLConfigParse(reinterpret_cast<TSYaml>(&root), field_handler, nullptr);
         if (ret != TS_SUCCESS) {
            TSError("We found an error while parsing '%s'.", path.c_str());
            return;
         }
      }
   ..


Return Values
=============

:c:func:`TSRecYAMLConfigParse`

:c:func:`TSRecYAMLConfigParse`  This will return :const:`TS_ERROR` if there was an issue while parsing the file. Particular node errors
should be handled by the `TSYAMLRecNodeHandler` implementation.

