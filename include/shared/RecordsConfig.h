/** @file

  Legacy Record structure to be used to set and validate the default record values.

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
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
 */

#pragma once

#include <unordered_map>
#include "records/P_RecCore.h"

// This is to manage the librecords table sizes. Not awesome, but better than the earlier recompiling of ATS requirement...
extern int max_records_entries;
namespace ts
{
enum RecordRequiredType {
  RR_NULL,    // config is _not_ required to be defined in records.config
  RR_REQUIRED // config _is_ required to be defined in record.config
};

// Retain this struct for ease of CVS merging
struct RecordElement {
  RecT type;                   // type of the record (CONFIG, PROCESS, etc)
  const char *name;            // name of the record
  RecDataT value_type;         // type of the record value (INT, FLOAT, etc)
  const char *value;           // default value for the record
  RecUpdateT update;           // action necessary to change a configuration
  RecordRequiredType required; // is records required to be in records.config?
  RecCheckT check;
  const char *regex;
  RecAccessT access; // access level of the record
};

typedef void (*RecordElementCallback)(const RecordElement *, void *);
void RecordsConfigIterate(RecordElementCallback, void *);

void LibRecordsConfigInit(); // initializes RecordsConfigIndex

const RecordElement *GetRecordElementByName(std::string_view name);
} // namespace ts
