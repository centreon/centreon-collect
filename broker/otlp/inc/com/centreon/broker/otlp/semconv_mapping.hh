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
  sum_non_monotonic  // UpDownCounter
};

/**
 * @brief Datapoint attribute the "instance" part of a structured perfdata
 * label binds to.
 *
 * Centreon perfdata labels follow `instance~sub1~sub2#metric.name`, so the
 * mountpoint / interface / device is carried in the label itself rather than
 * being guessable from the metric name.
 */
enum class instance_attribute {
  none,
  filesystem_mountpoint,  // system.filesystem.mountpoint
  network_interface,      // network.interface.name
  system_device,          // system.device
  cpu_logical_number,     // cpu.logical_number
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
  /* Multiplied into the raw value to reach the OTel unit: 0.01 for percent to
   * ratio, 0.001 for ms to s, 0.125 for bits to bytes. */
  double scale = 1.0;
  /* Static datapoint attributes required by the convention, e.g.
   * cpu.mode=user. */
  std::vector<std::pair<std::string, std::string>> attributes;
  /* True when the metric kept its Centreon identity because no semantic
   * convention applies, or because a required instance attribute was absent. */
  bool is_fallback = false;
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
 */
mapping map_metric(std::string_view perfdata_name,
                   std::string_view unit,
                   com::centreon::common::perfdata::data_type value_type);

/**
 * @brief Name of the companion metric carrying this metric's thresholds.
 *
 * Derived from the value metric so a dashboard can find it by string
 * construction, and so each threshold series keeps the unit of the value it
 * annotates. "centreon." is not repeated if already present.
 */
std::string threshold_metric_name(std::string_view emitted_name);

/**
 * @brief Name of the companion metric carrying this metric's min/max bounds.
 */
std::string bound_metric_name(std::string_view emitted_name);

/**
 * @brief Sanitize an arbitrary perfdata label into a legal OTel name segment.
 */
std::string sanitize(std::string_view raw);

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_SEMCONV_MAPPING_HH
