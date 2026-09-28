/**
 * Copyright 2026 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/broker/otlp/semconv_mapping.hh"

#include "com/centreon/common/file_system.hh"
#include "com/centreon/common/rapidjson_helper.hh"

using namespace com::centreon::broker::otlp;
using com::centreon::common::json_validator;
using com::centreon::common::perfdata;
using com::centreon::common::rapidjson_helper;
using com::centreon::common::read_file_content;
using com::centreon::exceptions::msg_fmt;

namespace {

constexpr std::string_view k_mapping_schema = R"(
{
  "$schema": "http://json-schema.org/draft-04/schema#",
  "title": "otlp output semantic convention mapping",
  "type": "object",
  "required": ["metrics"],
  "properties": {
    "metrics": {
      "type": "object",
      "additionalProperties": {
        "type": "object",
        "required": ["name"],
        "properties": {
          "description": { "type": "string" },
          "name": { "type": "string", "minLength": 1 },
          "unit": { "type": "string" },
          "instrument": {
            "type": "string",
            "enum": ["gauge", "sum_monotonic", "sum_non_monotonic"]
          },
          "scale": { "type": "number" },
          "attributes": {
            "type": "object",
            "additionalProperties": { "type": "string" }
          },
          "instance_attribute": { "type": "string" }
        },
        "additionalProperties": false
      }
    }
  }
}
)";

instrument parse_instrument(std::string_view s) {
  if (s == "sum_monotonic")
    return instrument::sum_monotonic;
  if (s == "sum_non_monotonic")
    return instrument::sum_non_monotonic;
  return instrument::gauge;
}

/**
 * @brief UCUM unit for a Centreon unit of measure, for fallback metrics.
 *
 * Fallback keeps the value unscaled, so the unit reported must be the one the
 * value is actually in.
 */
std::string ucum_unit(std::string_view centreon_unit) {
  if (centreon_unit.empty())
    return "1";
  if (centreon_unit == "%")
    return "%";
  if (centreon_unit == "B" || centreon_unit == "b")
    return "By";
  if (centreon_unit == "s")
    return "s";
  if (centreon_unit == "ms")
    return "ms";
  if (centreon_unit == "c")
    return "1";
  return std::string(centreon_unit);
}

}  // namespace

namespace com::centreon::broker::otlp {

decomposed_name decompose(std::string_view perfdata_name) {
  decomposed_name res;
  const std::size_t label = perfdata_name.find('#');
  if (label == std::string_view::npos) {
    res.metric = perfdata_name;
    return res;
  }

  res.metric = perfdata_name.substr(label + 1);
  const std::string_view full_instance = perfdata_name.substr(0, label);
  const std::size_t tilda = full_instance.find('~');
  if (tilda == std::string_view::npos) {
    res.instance = full_instance;
    return res;
  }

  res.instance = full_instance.substr(0, tilda);
  for (std::string_view sub :
       absl::StrSplit(full_instance.substr(tilda + 1), '~'))
    res.subinstances.push_back(sub);
  return res;
}

/**
 * @brief Normalize a perfdata metric name for the Centreon fallback namespace.
 *
 * Examples:
 * @code
 * sanitize("CPU Usage!")         // "cpu_usage"
 * sanitize("memory.usage.bytes") // "memory.usage.bytes"
 * sanitize("Disk / Usage (%)")   // "disk_usage"
 * sanitize("..a...b..")          // "a.b"
 * sanitize("!!!")                // "unnamed"
 * @endcode
 *
 * @note Different labels can produce the same result (e.g. "CPU Usage" and
 * "CPU-Usage"). This function normalizes names; it does not ensure uniqueness.
 *
 * @param raw Metric part of the perfdata label, without the instance prefix.
 * @return Normalized name, or "unnamed" when no ASCII letter or digit remains.
 */
std::string sanitize(std::string_view raw) {
  std::string out;
  out.reserve(raw.size());
  bool last_underscore = false;
  for (char c : raw) {
    if (absl::ascii_isalnum(static_cast<unsigned char>(c))) {
      out.push_back(absl::ascii_tolower(static_cast<unsigned char>(c)));
      last_underscore = false;
    } else if (c == '.') {
      // keep
      if (!out.empty() && out.back() != '.') {
        out.push_back('.');
        last_underscore = false;
      }
    } else if (!last_underscore && !out.empty()) {
      out.push_back('_');
      last_underscore = true;
    }
  }
  while (!out.empty() && (out.back() == '_' || out.back() == '.'))
    out.pop_back();
  return out.empty() ? std::string("unnamed") : out;
}

mapping_table::pointer mapping_table::from_json(std::string_view json_content) {
  static json_validator validator(k_mapping_schema);
  rapidjson::Document doc = rapidjson_helper::read_from_string(json_content);
  try {
    rapidjson_helper(doc).validate(validator);
  } catch (const std::invalid_argument& e) {
    throw msg_fmt("{}", e.what());
  }

  auto table = std::make_shared<mapping_table>();
  const rapidjson::Value& metrics = doc["metrics"];
  for (auto it = metrics.MemberBegin(); it != metrics.MemberEnd(); ++it) {
    rapidjson_helper row(it->value);
    mapping_rule rule;
    rule.name = row.get_string("name");
    rule.unit = row.get_string("unit", "");
    rule.instr = parse_instrument(row.get_string("instrument", "gauge"));
    rule.scale = row.has_member("scale") ? row.get_double("scale") : 1.0;
    if (row.has_member("attributes")) {
      const rapidjson::Value& attrs = row.get_member("attributes");
      for (auto a = attrs.MemberBegin(); a != attrs.MemberEnd(); ++a)
        rule.attributes.emplace_back(a->name.GetString(), a->value.GetString());
    }
    rule.instance_attribute = row.get_string("instance_attribute", "");
    table->_rules.emplace(it->name.GetString(), std::move(rule));
  }
  return table;
}

mapping_table::pointer mapping_table::from_file(
    const std::filesystem::path& path) {
  try {
    return from_json(read_file_content(path));
  } catch (const std::exception& e) {
    throw msg_fmt("invalid mapping file {}: {}", path.string(), e.what());
  }
}

const mapping_table::pointer& mapping_table::empty() {
  static const pointer table = std::make_shared<const mapping_table>();
  return table;
}

const mapping_rule* mapping_table::find(std::string_view metric) const {
  auto found = _rules.find(metric);
  return found == _rules.end() ? nullptr : &found->second;
}

mapping map_metric(std::string_view perfdata_name,
                   std::string_view unit,
                   perfdata::data_type value_type,
                   const mapping_table& table) {
  const decomposed_name parts = decompose(perfdata_name);

  if (const mapping_rule* rule = table.find(parts.metric)) {
    /* A convention that identifies its series by mountpoint/interface is
     * meaningless without it: every data would collapse into one
     * series. Degrade to the Centreon namespace instead. */
    if (rule->instance_attribute.empty() || !parts.instance.empty()) {
      mapping m;
      m.name = rule->name;
      m.unit = rule->unit;
      m.instr = rule->instr;
      m.scale = rule->scale;
      m.attributes = rule->attributes;
      if (!rule->instance_attribute.empty())
        m.attributes.emplace_back(rule->instance_attribute,
                                  std::string(parts.instance));
      return m;
    }
  }

  mapping m;
  m.name = absl::StrCat("centreon.", sanitize(parts.metric));
  m.unit = ucum_unit(unit);
  m.instr = (value_type == perfdata::counter || value_type == perfdata::derive)
                ? instrument::sum_monotonic
                : instrument::gauge;
  m.scale = 1.0;
  m.is_fallback = true;

  if (!parts.instance.empty())
    m.attributes.emplace_back("centreon.metric.instance",
                              std::string(parts.instance));
  return m;
}

}  // namespace com::centreon::broker::otlp
