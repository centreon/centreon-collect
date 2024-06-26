/**
 * Copyright 2014-2015,2019-2021 Centreon
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

#include "bbdo/bam/dimension_kpi_event.hh"

#include "com/centreon/broker/sql/table_max_size.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

static constexpr double eps = 0.000001;

/**
 *  Default constructor.
 */
dimension_kpi_event::dimension_kpi_event(uint32_t kpi_id)
    : io::data(dimension_kpi_event::static_type()),
      kpi_id(kpi_id),
      ba_id(0),
      host_id(0),
      service_id(0),
      kpi_ba_id(0),
      meta_service_id(0),
      boolean_id(0),
      impact_warning(0),
      impact_critical(0),
      impact_unknown(0) {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
dimension_kpi_event::dimension_kpi_event(dimension_kpi_event const& other)
    : io::data(other),
      kpi_id(other.kpi_id),
      ba_id(other.ba_id),
      ba_name(other.ba_name),
      host_id(other.host_id),
      host_name(other.host_name),
      service_id(other.service_id),
      service_description(other.service_description),
      kpi_ba_id(other.kpi_ba_id),
      kpi_ba_name(other.kpi_ba_name),
      meta_service_id(other.meta_service_id),
      meta_service_name(other.meta_service_name),
      boolean_id(other.boolean_id),
      boolean_name(other.boolean_name),
      impact_warning(other.impact_warning),
      impact_critical(other.impact_critical),
      impact_unknown(other.impact_unknown) {}

/**
 *  Equality test operator.
 *
 *  @param[in] other  The object to test for equality.
 *
 *  @return  True if the two objects are equal.
 */
bool dimension_kpi_event::operator==(dimension_kpi_event const& other) const {
  return kpi_id == other.kpi_id && ba_id == other.ba_id &&
         ba_name == other.ba_name && host_id == other.host_id &&
         host_name == other.host_name && service_id == other.service_id &&
         service_description == other.service_description &&
         kpi_ba_id == other.kpi_ba_id && kpi_ba_name == other.kpi_ba_name &&
         meta_service_id == other.meta_service_id &&
         meta_service_name == other.meta_service_name &&
         boolean_id == other.boolean_id && boolean_name == other.boolean_name &&
         std::abs(impact_warning - other.impact_warning) < eps &&
         std::abs(impact_critical - other.impact_critical) < eps &&
         std::abs(impact_unknown - other.impact_unknown) < eps;
}

// Mapping.
mapping::entry const dimension_kpi_event::entries[] = {
    mapping::entry(&bam::dimension_kpi_event::kpi_id,
                   "kpi_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&bam::dimension_kpi_event::ba_id,
                   "ba_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&bam::dimension_kpi_event::ba_name,
                   "ba_name",
                   get_centreon_storage_mod_bam_reporting_kpi_col_size(
                       centreon_storage_mod_bam_reporting_kpi_ba_name)),
    mapping::entry(&bam::dimension_kpi_event::host_id,
                   "host_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&bam::dimension_kpi_event::host_name,
                   "host_name",
                   get_centreon_storage_mod_bam_reporting_kpi_col_size(
                       centreon_storage_mod_bam_reporting_kpi_host_name)),
    mapping::entry(&bam::dimension_kpi_event::service_id,
                   "service_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(
        &bam::dimension_kpi_event::service_description,
        "service_description",
        get_centreon_storage_mod_bam_reporting_kpi_col_size(
            centreon_storage_mod_bam_reporting_kpi_service_description)),
    mapping::entry(&bam::dimension_kpi_event::kpi_ba_id,
                   "kpi_ba_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&bam::dimension_kpi_event::kpi_ba_name,
                   "kpi_ba_name",
                   get_centreon_storage_mod_bam_reporting_kpi_col_size(
                       centreon_storage_mod_bam_reporting_kpi_kpi_ba_name)),
    mapping::entry(&bam::dimension_kpi_event::meta_service_id,
                   "meta_service_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(
        &bam::dimension_kpi_event::meta_service_name,
        "meta_service_name",
        get_centreon_storage_mod_bam_reporting_kpi_col_size(
            centreon_storage_mod_bam_reporting_kpi_meta_service_name),
        mapping::entry::invalid_on_zero),
    mapping::entry(&bam::dimension_kpi_event::boolean_id,
                   "boolean_id",
                   mapping::entry::invalid_on_zero),
    mapping::entry(&bam::dimension_kpi_event::boolean_name,
                   "boolean_name",
                   get_centreon_storage_mod_bam_reporting_kpi_col_size(
                       centreon_storage_mod_bam_reporting_kpi_boolean_name),
                   mapping::entry::invalid_on_zero),
    mapping::entry(&bam::dimension_kpi_event::impact_warning, "impact_warning"),
    mapping::entry(&bam::dimension_kpi_event::impact_critical,
                   "impact_critical"),
    mapping::entry(&bam::dimension_kpi_event::impact_unknown, "impact_unknown"),
    mapping::entry()};

// Operations.
static io::data* new_dimension_kpi_event() {
  return new dimension_kpi_event(0);
}
io::event_info::event_operations const dimension_kpi_event::operations = {
    &new_dimension_kpi_event, nullptr, nullptr};
