/**
 * Copyright 2014-2026 Centreon
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

#include "com/centreon/broker/bam/configuration/state.hh"

#include "broker/core/config/applier/state.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam::configuration;

/**
 * @brief Constructor.
 *
 * Picks which of the two regimes answers the host/service questions, and it is
 * the only place where that choice is made. When Broker holds the
 * configurations of the pollers, the global cache already knows what BAM would
 * otherwise read from the configuration database, and it is then the only
 * source: nothing is loaded and nothing is kept. Otherwise BAM builds the
 * mapping for itself, as it always did.
 *
 * @param[in] logger The logger to hand to the mapping.
 */
state::state(const std::shared_ptr<spdlog::logger>& logger) {
  if (config::applier::state::instance().supports_centralized_conf())
    _hst_svc_mapping = std::make_unique<bam::global_hst_svc_mapping>(logger);
  else {
    auto local = std::make_unique<bam::local_hst_svc_mapping>(logger);
    _local_hst_svc_mapping = local.get();
    _hst_svc_mapping = std::move(local);
  }
}

/**
 *  Clear state.
 */
void state::clear() {
  _bas.clear();
  _kpis.clear();
  _bool_expressions.clear();
}

/**
 *  Get the list of BAs.
 *
 *  @return  A const list of all the business activities.
 */
const state::bas& state::get_bas() const {
  return _bas;
}

/**
 *  Get all the kpis.
 *
 *  @return  A const list of kpis.
 */
const state::kpis& state::get_kpis() const {
  return _kpis;
}

/**
 *  Get all the boolean expressions.
 *
 *  @return  A list of constant expressions.
 */
const state::bool_exps& state::get_bool_exps() const {
  return _bool_expressions;
}

/**
 *  Get host/service mapping.
 *
 *  @return Mapping.
 */
const bam::hst_svc_mapping& state::get_hst_svc_mapping() const {
  return *_hst_svc_mapping;
}

/**
 *  Get BA/service mapping.
 *
 *  @return Mapping.
 */
const bam::ba_svc_mapping& state::get_ba_svc_mapping() const {
  return _ba_svc_mapping;
}

/**
 *  Get all the business activities
 *
 *  @return  The list of all the business activities.
 */
state::bas& state::get_bas() {
  return _bas;
}

/**
 *  Get all the kpis
 *
 *  @return  A list of kpis.
 */
state::kpis& state::get_kpis() {
  return _kpis;
}

/**
 *  Get all the boolean expressions.
 *
 *  @return  A list of expressions.
 */
state::bool_exps& state::get_bool_exps() {
  return _bool_expressions;
}

/**
 *  Get host/service mapping.
 *
 *  @return Mapping.
 */
bam::hst_svc_mapping& state::get_hst_svc_mapping() {
  return *_hst_svc_mapping;
}

/**
 *  Get the mapping BAM fills for itself, when it is the one in use.
 *
 *  @return The mapping to fill, or nullptr when the global cache answers -- in
 *          which case there is nothing to fill.
 */
bam::local_hst_svc_mapping* state::get_local_hst_svc_mapping() {
  return _local_hst_svc_mapping;
}

/**
 *  Get BA/service mapping.
 *
 *  @return Mapping.
 */
bam::ba_svc_mapping& state::get_ba_svc_mapping() {
  return _ba_svc_mapping;
}
