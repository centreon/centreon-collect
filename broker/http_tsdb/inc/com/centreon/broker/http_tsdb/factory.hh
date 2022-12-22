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
#include "com/centreon/broker/namespace.hh"
#include "http_tsdb_config.hh"

CCB_BEGIN()

namespace http_tsdb {

class factory : public io::factory {
 protected:
  std::string _name;
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  virtual void create_conf(const config::endpoint& cfg,
                           http_tsdb_config& conf) const;

  std::string find_param(config::endpoint const& cfg,
                         std::string const& key) const;

 public:
  factory(const std::string& name,
          const std::shared_ptr<asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger);
  factory(factory const&) = delete;
  ~factory() = default;
  factory& operator=(factory const& other) = delete;
  bool has_endpoint(config::endpoint& cfg, io::extension* ext) override;
};
}  // namespace http_tsdb

CCB_END()

#endif  // !CCB_HTTP_TSDB_FACTORY_HH
