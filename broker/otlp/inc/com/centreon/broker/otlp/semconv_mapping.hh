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

#ifndef CCB_OTLP_SEMCONV_MAPPING_HH
#define CCB_OTLP_SEMCONV_MAPPING_HH

#include "com/centreon/common/perfdata.hh"

namespace com::centreon::broker::otlp {

/**
 * @brief OpenTelemetry instrument kind for an emitted metric.
 */
enum class instrument {
  gauge,
  sum_monotonic,     // cumulative counter
  sum_non_monotonic  // UpDown counter
};

/**
 * @brief Result of decomposing a raw perfdata label.
 *
 * The views point into the string passed to decompose(), which must outlive
 * the result.
 */
struct decomposed_name {
  std::string_view instance;
  std::vector<std::string_view> subinstances;
  std::string_view metric;
};

/**
 * @brief A resolved mapping from a Centreon perfdata label to an OTel metric.
 */
struct mapping {
  std::string name;
  std::string unit;
  instrument instr = instrument::gauge;
  double scale = 1.0;
  std::vector<std::pair<std::string, std::string>> attributes;
  bool is_fallback = false;
};

/**
 * @brief One row of the mapping table: how a Centreon metric name (the part
 * of a structured label after '#') becomes an OTel metric.
 */
struct mapping_rule {
  std::string name;
  std::string unit;
  instrument instr = instrument::gauge;
  double scale = 1.0;
  std::vector<std::pair<std::string, std::string>> attributes;
  /* Datapoint attribute the label's instance. Empty when the convention
   * needs no instance. When set and the label carries no instance, the rule is
   * not applied and the metric falls back to the centreon.* namespace. */
  std::string instance_attribute;
};

/**
 * @brief Centreon metric name -> OTel semantic convention, read from JSON.
 *
 * Immutable once built, so a table can be shared between the stream threads
 * and swapped as a whole when its file is reloaded.
 *
 * JSON format:
 * @code
 * {
 *   "metrics": {
 *     "disk.space.usage.bytes": {
 *       "name": "system.filesystem.usage",
 *       "unit": "By",
 *       "instrument": "sum_non_monotonic",
 *       "scale": 1,
 *       "attributes": { "system.filesystem.state": "used" },
 *       "instance_attribute": "system.filesystem.mountpoint"
 *     }
 *   }
 * }
 * @endcode
 * "instrument" is one of gauge (default), sum_monotonic, sum_non_monotonic;
 * "unit", "scale" (default 1), "attributes" and "instance_attribute" are
 * optional.
 */
class mapping_table {
  absl::flat_hash_map<std::string, mapping_rule> _rules;

 public:
  using pointer = std::shared_ptr<const mapping_table>;

  /**
   * @brief Parse and validate a mapping document.
   * @throw msg_fmt if the content is not valid JSON or not a valid mapping.
   */
  static pointer from_json(std::string_view json_content);

  /**
   * @brief Read, parse and validate a mapping file.
   * @throw msg_fmt if the file can't be read or is not a valid mapping.
   */
  static pointer from_file(const std::filesystem::path& path);

  /**
   * @brief Table with no rule: every metric falls back to centreon.*. Used
   * when no mapping file is configured.
   */
  static const pointer& empty();

  const mapping_rule* find(std::string_view metric) const;
  size_t size() const { return _rules.size(); }
};

/**
 * @brief Split `instance~sub1~sub2#metric.name` into its parts.
 *
 * A label with no '#' is entirely a metric name with no instance.
 */
decomposed_name decompose(std::string_view perfdata_name);

/**
 * @brief Resolve a perfdata label to the metric we emit for it.
 *
 * Total: a label with no semantic convention equivalent degrades to the
 * `centreon.*` namespace rather than failing. A label whose convention
 * requires an instance attribute it cannot supply degrades the same way,
 * because an under-attributed semconv metric silently aggregates unrelated
 * series (every filesystem into one).
 *
 * @param perfdata_name raw label, e.g. "/var#disk.space.usage.bytes"
 * @param unit perfdata unit of measure, e.g. "%" or "B"
 * @param value_type perfdata data type, decides gauge vs sum on fallback
 * @param table rules to resolve the label with
 */
mapping map_metric(std::string_view perfdata_name,
                   std::string_view unit,
                   com::centreon::common::perfdata::data_type value_type,
                   const mapping_table& table);

/**
 * @brief Sanitize an arbitrary perfdata label into a legal OTel name segment.
 */
std::string sanitize(std::string_view raw);

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_SEMCONV_MAPPING_HH