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

#ifndef CCB_HTTP_TSDB_FACTORY_HH
#define CCB_HTTP_TSDB_FACTORY_HH

#include "com/centreon/broker/io/factory.hh"

#include "http_tsdb_config.hh"

namespace com::centreon::broker {

namespace http_tsdb {

/**
 * @brief this class is not a final io_factory,
 * it doesn't implement new_endpoint
 * it only provide create_conf and find param to do a first fill of a
 * http_tsdb_config bean
 *
 */
class factory : public io::factory {
 protected:
  std::string _name;
  std::shared_ptr<asio::io_context> _io_context;

  void create_conf(const config::endpoint& cfg, http_tsdb_config& conf) const;

  std::string find_param(config::endpoint const& cfg,
                         std::string const& key) const;

 public:
  factory(const std::string& name,
          const std::shared_ptr<asio::io_context>& io_context);
  factory(factory const&) = delete;
  ~factory() = default;

  static std::vector<column> get_columns(const nlohmann::json& cfg);
  factory& operator=(factory const& other) = delete;
  bool has_endpoint(config::endpoint& cfg, io::extension* ext) override;
};
}  // namespace http_tsdb

}  // namespace com::centreon::broker

#endif  // !CCB_HTTP_TSDB_FACTORY_HH
