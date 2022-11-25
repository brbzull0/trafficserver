/** @file

  Decode the records.yaml configuration file.

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

#include "records/P_RecCore.h"
#include "records/RecYAMLDecoder.h"
#include "records/RecYAMLDefs.h"

#include "tscore/Diags.h"

#include "shared/RecordsConfig.h"

#include <string_view>

#include <swoc/Errata.h>
#include <swoc/BufferWriter.h>

static std::array<std::string_view, 5> Node_Type_to_Str{{"Undefined", "Null", "Scalar", "Sequence", "Map"}};
namespace
{
constexpr std::string_view CONFIG_RECORD_PREFIX{"proxy.config"};
const inline std::string RECORD_YAML_ROOT_STR{"ts"};

// enum class TagMappping { NULL, BOOL, INT FLOAT, STR };
constexpr std::string_view YAML_FLOAT_TAG_URI{"tag:yaml.org,2002:float"};
constexpr std::string_view YAML_INT_TAG_URI{"tag:yaml.org,2002:int"};
constexpr std::string_view YAML_STR_TAG_URI{"tag:yaml.org,2002:str"};
constexpr std::string_view YAML_BOOL_TAG_URI{"tag:yaml.org,2002:bool"};
constexpr std::string_view YAML_NULL_TAG_URI{"tag:yaml.org,2002:null"};

} // namespace

namespace detail
{
void flatten_node(CfgNode const &field, YAMLNodeHandler handler, swoc::Errata &errata);
std::pair<RecDataT, std::string> try_deduce_type(YAML::Node const &node);

struct scoped_cond_lock {
  scoped_cond_lock(bool lock = false) : _lock(lock)
  {
    if (_lock) {
      ink_rwlock_wrlock(&g_records_rwlock);
    }
  }
  ~scoped_cond_lock()
  {
    if (_lock) {
      ink_rwlock_unlock(&g_records_rwlock);
    }
  }
  bool _lock{false};
};
} // namespace detail

void
YAMLNodeToRecord(const CfgNode &field, swoc::Errata &errata)
{
  std::string record_name{field.get_record_name()};
  RecT rec_type{RecT::RECT_CONFIG};
  RecDataT data_type{RecDataT::RECD_NULL};

  // this function (ts::GetRec..) should be generic and possibly getting the value either
  // from where itgets it right now or a schema file.
  if (const auto *found = ts::GetRecordElementByName(record_name.c_str()); found) {
    if (REC_TYPE_IS_STAT(found->type)) {
      ink_release_assert(REC_TYPE_IS_STAT(found->type));
    }
    rec_type  = found->type;
    data_type = found->value_type;
  } else {
    std::string field_name = field.node.as<std::string>();

    // Not registered in ATS, could be a plugin or a invalid records. Let's use the node to get the type.
    auto [dtype, e] = detail::try_deduce_type(field.value_node);
    if (!e.empty()) {
      errata.note(ERRATA_WARN, "Ignoring field '{}' at {}. Not registered and {}", field_name, field.node.Mark(), e);
      // we can't continue without knowing the type. Should we do our best and try to detect it?
      return;
    }
    data_type = dtype;
  }

  std::string nvalue = field.value_node.as<std::string>();
  // OK, we parsed the record, send it to the handler ...
  std::string value_str = RecConfigOverrideFromEnvironment(record_name.c_str(), nvalue.c_str());
  RecSourceT source     = (nvalue == value_str ? REC_SOURCE_EXPLICIT : REC_SOURCE_ENV);

  if (source == REC_SOURCE_ENV) {
    errata.note(ERRATA_DEBUG, "'{}' was override with '{}' using an env variable", record_name, value_str);
  }
  RecData data;
  memset(&data, 0, sizeof(RecData));
  RecDataSetFromString(data_type, &data, value_str.c_str());
  RecSetRecord(rec_type, record_name.c_str(), data_type, &data, nullptr, source, false);
  RecDataZero(data_type, &data);
};

swoc::Errata
ParseRecordsFromYAML(YAML::Node root, YAMLNodeHandler handler, bool lock /*false by default*/)
{
  [[maybe_unused]] detail::scoped_cond_lock cond_lock(lock);

  swoc::Errata errata;
  if (YAML::NodeType::Map != root.Type()) {
    return swoc::Errata(ERRATA_NOTE, "{} was expected to be a map, got {} instead.", root, root.Type());
  }

  if (auto ts = root[RECORD_YAML_ROOT_STR]; ts.size()) {
    for (auto &&n : ts) {
      detail::flatten_node({n.first, n.second, CONFIG_RECORD_PREFIX}, handler, errata);
    }
  } else {
    return swoc::Errata(ERRATA_NOTE, "'ts' root key not present or no fields to read.");
  }

  return errata;
}

namespace detail
{
std::pair<RecDataT, std::string>
try_deduce_type(YAML::Node const &node)
{
  // Using the tag.
  std::string_view tag = node.Tag();
  if (tag == YAML_FLOAT_TAG_URI) {
    return {RecDataT::RECD_FLOAT, {}};
  } else if (tag == YAML_INT_TAG_URI) {
    return {RecDataT::RECD_INT, {}};
  } else if (tag == YAML_STR_TAG_URI) {
    return {RecDataT::RECD_STRING, {}};
  } else if (tag == YAML_INT_TAG_URI) {
    return {RecDataT::RECD_INT, {}};
  } else if (tag == YAML_BOOL_TAG_URI) {
    return {RecDataT::RECD_INT, {}};
  } else if (tag == YAML_NULL_TAG_URI) {
    return {RecDataT::RECD_NULL, {}};
  }
  std::string text;
  return {RecDataT::RECD_NULL, swoc::bwprint(text, "Unknown tag type '{}'", tag)};
}

/**
 * A very well explanation about this.
 */
// std::tuple<CfgNode, YAML::Node>
void
flatten_node(CfgNode const &field, YAMLNodeHandler handler, swoc::Errata &errata)
{
  switch (field.value_node.Type()) {
  case YAML::NodeType::Map: {
    field.append_field_name();
    for (auto &it : field.value_node) {
      flatten_node({it.first, it.second, field.get_record_name()}, handler, errata);
    }
  } break;
  case YAML::NodeType::Sequence:
  case YAML::NodeType::Scalar: {
    field.append_field_name();
    handler(field, errata);
  } break;
  default:; // done
  }
}
} // namespace detail
namespace swoc
{
BufferWriter &
bwformat(BufferWriter &w, bwf::Spec const &spec, YAML::NodeType::value type)
{
  return w.write(Node_Type_to_Str[type]);
}
BufferWriter &
bwformat(BufferWriter &w, bwf::Spec const &spec, YAML::Node const &node)
{
  return w.write(node.as<std::string>());
}
BufferWriter &
bwformat(BufferWriter &w, bwf::Spec const &spec, YAML::Mark const &mark)
{
  return w.print("Ln {}", mark.line + 1);
}
} // namespace swoc
