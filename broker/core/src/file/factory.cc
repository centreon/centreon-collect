/**
 * Copyright 2011-2013,2016-2024 Centreon
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

#include "com/centreon/broker/file/factory.hh"

#include "com/centreon/broker/file/opener.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::file;

/**
 *  Check if a configuration match the file layer.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  @return True if configuration matches the file layer.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  bool retval;
  if (ext)
    *ext = io::extension("FILE", false, false);
  if (cfg.type == "file") {
    cfg.params["coarse"] = "yes";  // File won't respond to any salutation.
    retval = true;
  } else
    retval = false;
  return retval;
}

/**
 *  Generate an endpoint matching a configuration.
 *
 *  @param[in]  cfg          Endpoint configuration.
 *  @param[out] is_acceptor  Will be set to false.
 *  @param[in]  cache        Unused.
 *
 *  @return Acceptor matching configuration.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache [[maybe_unused]]) const {
  // Find path to the file.
  std::string filename;
  {
    auto it = cfg.params.find("path");
    if (it == cfg.params.end())
      throw msg_fmt("file: no 'path' defined for file endpoint '{}'", cfg.name);
    filename = it->second;
  }

  // Generate opener.
  std::unique_ptr<opener> openr(new opener);
  openr->set_filename(filename);
  is_acceptor = false;
  return openr.release();
}
