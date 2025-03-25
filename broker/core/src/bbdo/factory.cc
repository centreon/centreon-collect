/**
 * Copyright 2013,2015,2017 Centreon
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

#include <absl/strings/match.h>

#include "com/centreon/broker/bbdo/acceptor.hh"
#include "com/centreon/broker/bbdo/connector.hh"
#include "com/centreon/broker/bbdo/factory.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/config/parser.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bbdo;
using com::centreon::common::log_v2::log_v2;

/**
 *  @brief Check if a configuration supports this protocol.
 *
 *  The endpoint 'protocol' tag must have the 'bbdo' value.
 *
 *  @param[in] cfg       Object configuration.
 *
 *  @return True if the configuration has this protocol.
 */
bool factory::has_endpoint(config::endpoint& cfg, io::extension* ext) {
  /* Legacy case: 'protocol' is set in the object and should be equal to "bbdo"
   */
  bool bbdo_protocol_found = false;
  auto it = cfg.params.find("protocol");
  bbdo_protocol_found = (it != cfg.params.end() && it->second == "bbdo");

  /* New case: with bbdo_client and bbdo_server, bbdo is automatic. */
  if (!bbdo_protocol_found)
    bbdo_protocol_found =
        (cfg.type == "bbdo_client" || cfg.type == "bbdo_server");

  if (ext)
    *ext = io::extension("BBDO", false, false);

  return bbdo_protocol_found;
}

/**
 *  Create a new endpoint from a configuration.
 *
 *  @param[in]  cfg         Endpoint configuration.
 *  @param[out] is_acceptor Set to true if the endpoint is an acceptor. When we
 *  enter in this function, substreams have already been checked. So this param
 *  has already a value.
 *  For this case (bbdo::factory), is_acceptor is set to false in case of a
 *  tcp::stream configured as acceptor but with the one peer retention flag.
 *  This is because "is_acceptor" in that case, means is input.
 *  @param[in]  cache       Unused.
 *
 *  @return Endpoint matching configuration.
 */
io::endpoint* factory::new_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params [[maybe_unused]],
    bool& is_acceptor,
    std::shared_ptr<persistent_cache> cache [[maybe_unused]]) const {
  // Return value.
  std::unique_ptr<io::endpoint> retval;

  auto logger = log_v2::instance().get(log_v2::CORE);
  // Coarse endpoint ?
  bool coarse = false;
  auto it = cfg.params.find("coarse");
  if (it != cfg.params.end()) {
    if (!absl::SimpleAtob(it->second, &coarse)) {
      logger->error(
          "factory: cannot parse the 'coarse' boolean: the content is '{}'",
          it->second);
      coarse = false;
    }
  }

  // only grpc serialization?
  bool grpc_serialized = direct_grpc_serialized(cfg);

  // Negotiation allowed ?
  bool negotiate = false;
  std::list<std::shared_ptr<io::extension>> extensions;
  if (!coarse) {
    it = cfg.params.find("negotiation");
    if (it == cfg.params.end() || it->second != "no")
      negotiate = true;
    extensions = _extensions(cfg);
  }

  // Ack limit.
  uint32_t ack_limit{1000};
  it = cfg.params.find("ack_limit");
  if (it != cfg.params.end() && !absl::SimpleAtoi(it->second, &ack_limit)) {
    logger->error("BBDO: Bad value for ack_limit, it must be an integer.");
    ack_limit = 1000;
  }

  // Create object.
  std::string host;

  it = cfg.params.find("host");
  if (it != cfg.params.end())
    host = it->second;

  /* The substream is an acceptor (usually the substream a TCP acceptor */
  if (is_acceptor) {
    // This flag is just needed to know if we keep the retention or not...
    // When the connection is made to the map server, no retention is needed,
    // otherwise we want it.
    bool keep_retention{false};
    it = cfg.params.find("retention");
    if (it != cfg.params.end()) {
      if (cfg.type == "bbdo_server") {
        if (!absl::SimpleAtob(it->second, &keep_retention)) {
          log_v2::instance()
              .get(log_v2::CORE)
              ->error(
                  "BBDO: cannot parse the 'retention' boolean: its content is "
                  "'{}'",
                  it->second);
          keep_retention = false;
        }
      } else {
        logger->error(
            "BBDO: Configuration error, the 'retention' mode should be "
            "set only on a bbdo_server");
        keep_retention = false;
      }
    }

    it = cfg.params.find("one_peer_retention_mode");
    if (it != cfg.params.end()) {
      if (!absl::SimpleAtob(it->second, &keep_retention)) {
        logger->error(
            "BBDO: cannot parse the 'one_peer_retention_mode' boolean: the "
            "content is '{}'",
            it->second);
        keep_retention = false;
      }
    }

    // One peer retention mode? (i.e. keep_retention + acceptor_is_output)
    bool acceptor_is_output = cfg.get_io_type() == config::endpoint::output;
    if (!acceptor_is_output && keep_retention)
      logger->error(
          "BBDO: Configuration error, the one peer retention mode should "
          "be "
          "set only when the connection is reversed");

    retval = std::make_unique<bbdo::acceptor>(
        cfg.name, negotiate, cfg.read_timeout, acceptor_is_output, coarse,
        ack_limit, std::move(extensions), grpc_serialized);
    if (acceptor_is_output && keep_retention)
      is_acceptor = false;
    logger->debug("BBDO: new acceptor {}", cfg.name);
  } else {
    bool connector_is_input = cfg.get_io_type() == config::endpoint::input;
    retval = std::make_unique<bbdo::connector>(
        negotiate, cfg.read_timeout, connector_is_input, coarse, ack_limit,
        std::move(extensions), grpc_serialized);
    logger->debug("BBDO: new connector {}", cfg.name);
  }
  return retval.release();
}

/**
 *  Get available extensions for an endpoint. Two strings are returned:
 *  * the first one contains all the extensions supported by the endpoint.
 *  * the second one contains the mandatory extensions. This one is needed
 *    to report errors.
 *
 *  @param[in] cfg  Endpoint configuration.
 *
 *  return a pair of two strings, extensions and mandatories.
 */
std::list<std::shared_ptr<io::extension>> factory::_extensions(
    config::endpoint& cfg) const {
  std::list<std::shared_ptr<io::extension>> retval;

  for (std::map<std::string, io::protocols::protocol>::const_iterator
           it{io::protocols::instance().begin()},
       end{io::protocols::instance().end()};
       it != end; ++it) {
    std::shared_ptr<io::extension> ext = std::make_shared<io::extension>();
    bool has = it->second.endpntfactry->has_endpoint(cfg, &*ext);
    if (it->second.osi_from > 1 && it->second.osi_to < 7 &&
        (has || ext->is_mandatory() || ext->is_optional())) {
      retval.push_back(ext);
    }
  }
  return retval;
}
