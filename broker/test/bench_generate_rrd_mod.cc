/**
* Copyright 2013 Centreon
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

#include "test/bench_generate_rrd_mod.hh"
#include <QDomDocument>
#include <QDomElement>
#include "com/centreon/broker/config/state.hh"

static benchmark* thread(NULL);

/**
 *  Get parameter.
 *
 *  @param[in] param The param on xml format.
 *
 *  @return The value.
 */
static uint32_t _get_param(std::string const& param) {
  // Parse XML.
  QDomDocument d;
  if (d.setContent(static_cast<QString>(param.c_str()))) {
    // Browse first-level elements.
    QDomElement elem(d.documentElement());
    if (!elem.isNull())
      return (elem.text().toUInt());
  }
  return (0);
}

extern "C" {
/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  delete thread;
  thread = NULL;
  return true;  // ok to be unloaded
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(config::state const* cfg) {
  std::map<std::string, std::string>::const_iterator it1, it2;
  it1 = cfg->params().find("bench_services");
  it2 = cfg->params().find("bench_requests_per_service");
  if (it1 != cfg->params().end() && it2 != cfg->params().end()) {
    uint32_t services(_get_param(it1->second));
    uint32_t requests_per_service(_get_param(it2->second));
    thread = new benchmark(services, requests_per_service);
    thread->start();
  }
  return;
}
}
