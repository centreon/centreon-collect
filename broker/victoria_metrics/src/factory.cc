/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/victoria_metrics/factory.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/broker/victoria_metrics/connector.hh"
#include "com/centreon/broker/victoria_metrics/victoria_config.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;

factory::factory()
    : http_tsdb::factory("victoria_metrics", pool::io_context_ptr()) {}

io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache) const {
  is_acceptor = false;

  std::string target = "/write";
  std::map<std::string, std::string>::const_iterator it{
      cfg.params.find("http_target")};
  if (it != cfg.params.end()) {
    target = it->second;
  }

  std::shared_ptr<victoria_config> conf(
      std::make_shared<victoria_config>(target));

  create_conf(cfg, *conf);
  return new connector(conf, cache);
}