/**
* Copyright 2014-2015 Centreon
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

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam::configuration;

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
state::bas const& state::get_bas() const {
  return _bas;
}

/**
 *  Get all the kpis.
 *
 *  @return  A const list of kpis.
 */
state::kpis const& state::get_kpis() const {
  return _kpis;
}

/**
 *  Get all the boolean expressions.
 *
 *  @return  A list of constant expressions.
 */
state::bool_exps const& state::get_bool_exps() const {
  return _bool_expressions;
}

/**
 *  Get host/service mapping.
 *
 *  @return Mapping.
 */
bam::hst_svc_mapping const& state::get_hst_svc_mapping() const {
  return _hst_svc_mapping;
}

/**
 *  Get BA/service mapping.
 *
 *  @return Mapping.
 */
bam::ba_svc_mapping const& state::get_ba_svc_mapping() const {
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
  return _hst_svc_mapping;
}

/**
 *  Get BA/service mapping.
 *
 *  @return Mapping.
 */
bam::ba_svc_mapping& state::get_ba_svc_mapping() {
  return _ba_svc_mapping;
}
