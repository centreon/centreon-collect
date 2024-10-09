/**
 * Copyright 2022-2024 Centreon
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

#include "com/centreon/broker/victoria_metrics/factory.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/victoria_metrics/connector.hh"
#include "com/centreon/common/pool.hh"

using namespace nlohmann;
using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;
using namespace nlohmann::literals;

const json factory::default_extra_status_column = R"([
    {"name" : "host", "is_tag" : "true", "value" : "$HOST$", "type":"string"},
    {"name" : "serv", "is_tag" : "true", "value" : "$SERVICE$", "type":"string"},
    {"name" : "host_grp", "is_tag" : "true", "value" : "$HOSTGROUP$", "type":"string"},
    {"name" : "serv_grp", "is_tag" : "true", "value" : "$SERVICE_GROUP$", "type":"string"},
    {"name" : "host_tag_cat", "is_tag" : "true", "value" : "$HOST_TAG_CAT_NAME$", "type":"string"},
    {"name" : "host_tag_grp", "is_tag" : "true", "value" : "$HOST_TAG_GROUP_NAME$", "type":"string"},
    {"name" : "serv_tag_cat", "is_tag" : "true", "value" : "$SERV_TAG_CAT_NAME$", "type":"string"},
    {"name" : "serv_tag_grp", "is_tag" : "true", "value" : "$SERV_TAG_GROUP_NAME$", "type":"string"}])"_json;

const json factory::default_extra_metric_column = R"([
    {"name" : "host", "is_tag" : "true", "value" : "$HOST$", "type":"string"},
    {"name" : "serv", "is_tag" : "true", "value" : "$SERVICE$", "type":"string"},
    {"name" : "min", "is_tag" : "true", "value" : "$MIN$", "type":"number"},
    {"name" : "max", "is_tag" : "true", "value" : "$MAX$", "type":"number"},
    {"name" : "host_grp", "is_tag" : "true", "value" : "$HOSTGROUP$", "type":"string"},
    {"name" : "serv_grp", "is_tag" : "true", "value" : "$SERVICE_GROUP$", "type":"string"},
    {"name" : "host_tag_cat", "is_tag" : "true", "value" : "$HOST_TAG_CAT_NAME$", "type":"string"},
    {"name" : "host_tag_grp", "is_tag" : "true", "value" : "$HOST_TAG_GROUP_NAME$", "type":"string"},
    {"name" : "serv_tag_cat", "is_tag" : "true", "value" : "$SERV_TAG_CAT_NAME$", "type":"string"},
    {"name" : "serv_tag_grp", "is_tag" : "true", "value" : "$SERV_TAG_GROUP_NAME$", "type":"string"}])"_json;

factory::factory()
    : http_tsdb::factory("victoria_metrics",
                         com::centreon::common::pool::io_context_ptr()) {}

io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params
    [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache>) const {
  is_acceptor = false;

  std::shared_ptr<http_tsdb::http_tsdb_config> conf(
      std::make_shared<http_tsdb::http_tsdb_config>());
  // default extra columns for victoria
  if (cfg.cfg.find("status_column") == cfg.cfg.end() ||
      cfg.cfg.find("metrics_column") == cfg.cfg.end()) {
    config::endpoint cfg_copy(cfg);
    if (cfg_copy.cfg.find("status_column") == cfg_copy.cfg.end()) {
      cfg_copy.cfg["status_column"] = default_extra_status_column;
    }
    if (cfg_copy.cfg.find("metrics_column") == cfg_copy.cfg.end()) {
      cfg_copy.cfg["metrics_column"] = default_extra_metric_column;
    }
    create_conf(cfg_copy, *conf);
  } else {
    create_conf(cfg, *conf);
  }
  std::string account_id;
  auto it = cfg.params.find("account_id");
  if (it != cfg.params.end()) {
    account_id = it->second;
  }

  return new connector(conf, account_id);
}
