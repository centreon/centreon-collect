/**
 * Copyright 2011-2012,2015,2017-2022 Centreon
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

#include "com/centreon/broker/config/applier/endpoint.hh"

#include <cassert>

#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/processing/failover.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::config::applier;

using log_v2 = com::centreon::common::log_v2::log_v2;

// Class instance.
static config::applier::endpoint* gl_endpoint = nullptr;
static std::atomic_bool gl_loaded{false};

/**
 * @brief Default constructor.
 */
endpoint::endpoint()
    : _discarding{false}, _logger{log_v2::instance().get(log_v2::CONFIG)} {}

/**
 *  Comparison classes.
 */
class failover_match_name {
 public:
  failover_match_name(std::string const& fo) : _failover(fo) {}
  failover_match_name(failover_match_name const& fmn)
      : _failover(fmn._failover) {}
  ~failover_match_name() {}
  failover_match_name& operator=(failover_match_name& fmn) {
    _failover = fmn._failover;
    return *this;
  }
  bool operator()(config::endpoint const& endp) const {
    return _failover == endp.name;
  }

 private:
  std::string _failover;
};
class name_match_failover {
 public:
  name_match_failover(std::string const& name) : _name(name) {}
  name_match_failover(name_match_failover const& nmf) : _name(nmf._name) {}
  ~name_match_failover() {}
  name_match_failover& operator=(name_match_failover const& nmf) {
    _name = nmf._name;
    return *this;
  }
  bool operator()(config::endpoint const& endp) const {
    return (std::find(endp.failovers.begin(), endp.failovers.end(), _name) !=
            endp.failovers.end());
  }

 private:
  std::string _name;
};

/**
 *  Destructor.
 */
endpoint::~endpoint() {
  _discard();
}

/**
 *  Apply the endpoint configuration.
 *
 *  @param[in] endpoints  Endpoints configuration objects.
 */
void endpoint::apply(std::list<config::endpoint> const& endpoints,
                     const std::map<std::string, std::string>& global_params) {
  // Log messages.
  SPDLOG_LOGGER_INFO(_logger, "endpoint applier: loading configuration");

  if (_logger->level() <= spdlog::level::debug) {
    std::vector<std::string> eps;
    for (auto& ep : endpoints)
      eps.push_back(ep.name);
    SPDLOG_LOGGER_DEBUG(_logger, "endpoint applier: {} endpoints to apply: {}",
                        endpoints.size(),
                        fmt::format("{}", fmt::join(eps, ", ")));
  }

  // Copy endpoint configurations and apply eventual modifications.
  std::list<config::endpoint> tmp_endpoints(endpoints);

  // Remove old inputs and generate inputs to create.
  std::list<config::endpoint> endp_to_create;
  {
    std::lock_guard<std::timed_mutex> lock(_endpointsm);
    std::list<config::endpoint> endp_to_delete;
    _diff_endpoints(_endpoints, tmp_endpoints, endp_to_create, endp_to_delete);

    // Remove old endpoints.
    for (auto& ep : endp_to_delete) {
      // Send only termination request, object will be destroyed by event
      // loop on termination. But wait for threads because they hold
      // resources that might be used by other endpoints.
      auto it = _endpoints.find(ep);
      if (it != _endpoints.end()) {
        SPDLOG_LOGGER_DEBUG(_logger,
                            "endpoint applier: removing old endpoint {}",
                            it->first.name);
        /* failover::exit() is called. */
        it->second->exit();
        delete it->second;
        _endpoints.erase(it);
      }
    }
  }

  // Update existing endpoints.
  for (auto it = _endpoints.begin(), end = _endpoints.end(); it != end; ++it) {
    SPDLOG_LOGGER_DEBUG(_logger, "endpoint applier: updating endpoint {}",
                        it->first.name);
    it->second->update();
  }

  // Debug message.
  SPDLOG_LOGGER_DEBUG(_logger, "endpoint applier: {} endpoints to create",
                      endp_to_create.size());

  // Create new endpoints.
  for (config::endpoint& ep : endp_to_create) {
    assert(!_discarding);
    // Check that output is not a failover.
    if (ep.name.empty() ||
        std::find_if(endp_to_create.begin(), endp_to_create.end(),
                     name_match_failover(ep.name)) == endp_to_create.end()) {
      SPDLOG_LOGGER_DEBUG(_logger, "endpoint applier: creating endpoint {}",
                          ep.name);
      bool is_acceptor;
      std::shared_ptr<io::endpoint> e{
          _create_endpoint(ep, global_params, is_acceptor)};
      std::unique_ptr<processing::endpoint> endp;
      /* Input or output? */
      /* This is tricky, one day we will make better... I hope.
       * In case of an Engine making connection to Broker, usually Broker is an
       * acceptor and Engine not.
       * In case of one peer retention, each one keeps its role but the
       * connection is reversed. To keep this behavior, Engine is still
       * considered as the connector and Broker the acceptor, is_acceptor is
       * then set to false.
       * In case of Broker connected to Map, Broker is a TCP acceptor, and
       * this flag is returned as true. And then we create an acceptor even
       * if broker sends data to map. This is needed because a failover needs
       * its peer to ack events to release them (and a failover is also able
       * to write data). */
      multiplexing::muxer_filter r_filter =
          parse_filters(ep.read_filters, e->get_stream_forbidden_filter());
      multiplexing::muxer_filter w_filter =
          parse_filters(ep.write_filters, e->get_stream_forbidden_filter());
      if (is_acceptor) {
        w_filter -= e->get_stream_forbidden_filter();
        r_filter -= e->get_stream_forbidden_filter();
        std::unique_ptr<processing::acceptor> acceptr(
            std::make_unique<processing::acceptor>(e, ep.name, r_filter,
                                                   w_filter));
        SPDLOG_LOGGER_DEBUG(
            _logger,
            "endpoint applier: acceptor '{}' configured with write filters: {} "
            "and read filters: {}",
            ep.name, w_filter.get_allowed_categories(),
            r_filter.get_allowed_categories());
        endp.reset(acceptr.release());
      } else {
        // Create muxer and endpoint.

        /* Are there missing events in the w_filter ? */
        if (!e->get_stream_mandatory_filter().is_in(w_filter)) {
          w_filter |= e->get_stream_mandatory_filter();
          SPDLOG_LOGGER_DEBUG(
              _logger,
              "endpoint applier: The configured write filters for the endpoint "
              "'{}' are too restrictive. Mandatory categories added to them",
              ep.name);
        }
        /* Are there events in w_filter that are forbidden ? */
        if (w_filter.contains_some_of(e->get_stream_forbidden_filter())) {
          w_filter -= e->get_stream_forbidden_filter();
          SPDLOG_LOGGER_ERROR(
              _logger,
              "endpoint applier: The configured write filters for the endpoint "
              "'{}' contain forbidden filters. These ones are removed",
              ep.name);
        }

        /* Are there events in r_filter that are forbidden ? */
        if (r_filter.contains_some_of(e->get_stream_forbidden_filter())) {
          r_filter -= e->get_stream_forbidden_filter();
          SPDLOG_LOGGER_ERROR(
              _logger,
              "endpoint applier: The configured read filters for the endpoint "
              "'{}' contain forbidden filters. These ones are removed",
              ep.name);
        }
        SPDLOG_LOGGER_DEBUG(
            _logger, "endpoint applier: filters {} for endpoint '{}' applied.",
            w_filter.get_allowed_categories(), ep.name);

        auto mux = multiplexing::muxer::create(
            ep.name, multiplexing::engine::instance_ptr(), r_filter, w_filter,
            true);
        endp.reset(_create_failover(ep, global_params, mux, e, endp_to_create));
      }
      {
        std::lock_guard<std::timed_mutex> lock(_endpointsm);
        _endpoints[ep] = endp.get();
      }

      // Run thread.
      SPDLOG_LOGGER_DEBUG(
          _logger,
          "endpoint applier: endpoint thread {} of '{}' is registered and "
          "ready to run",
          static_cast<void*>(endp.get()), ep.name);
      endp.release()->start();
    }
  }
}

/**
 *  Discard applied configuration. Running endpoints are destroyed one by one.
 *
 */
void endpoint::_discard() {
  _discarding = true;
  SPDLOG_LOGGER_DEBUG(_logger, "endpoint applier: destruction");

  // wait for failover and feeder to push endloop event
  ::usleep(processing::idle_microsec_wait_idle_thread_delay + 100000);
  // Exit threads.
  {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "endpoint applier: requesting threads termination");
    std::unique_lock<std::timed_mutex> lock(_endpointsm);

    // Send termination requests.
    // We begin with feeders
    for (auto it = _endpoints.begin(); it != _endpoints.end();) {
      if (it->second->is_feeder()) {
        it->second->wait_for_all_events_written(5000);
        SPDLOG_LOGGER_TRACE(
            _logger, "endpoint applier: send exit signal to endpoint '{}'",
            it->second->get_name());
        delete it->second;
        it = _endpoints.erase(it);
      } else
        ++it;
    }
  }

  // Exit threads.
  {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "endpoint applier: requesting threads termination");
    std::unique_lock<std::timed_mutex> lock(_endpointsm);

    // We continue with failovers
    for (auto it = _endpoints.begin(); it != _endpoints.end();) {
      it->second->wait_for_all_events_written(5000);
      SPDLOG_LOGGER_TRACE(_logger,
                          "endpoint applier: send exit signal on endpoint '{}'",
                          it->second->get_name());
      delete it->second;
      it = _endpoints.erase(it);
    }

    SPDLOG_LOGGER_DEBUG(_logger,
                        "endpoint applier: all threads are terminated");
  }

  // Stop multiplexing: we must stop the engine after failovers otherwise
  // the stop/pb_stop message cannot go.
  try {
    multiplexing::engine::instance_ptr()->stop();
  } catch (const std::exception& e) {
    _logger->warn("multiplexing engine stop interrupted: {}", e.what());
  }
}

/**
 *  Get iterator to the beginning of endpoints.
 *
 *  @return Iterator to the first endpoint.
 */
endpoint::iterator endpoint::endpoints_begin() {
  return _endpoints.begin();
}

/**
 *  Get last iterator of endpoints.
 *
 *  @return Last iterator of endpoints.
 */
endpoint::iterator endpoint::endpoints_end() {
  return _endpoints.end();
}

/**
 *  Get endpoints mutex.
 *
 *  @return Endpoints mutex.
 */
std::timed_mutex& endpoint::endpoints_mutex() {
  return _endpointsm;
}

/**
 *  Get the class instance.
 *
 *  @return Class instance.
 */
endpoint& endpoint::instance() {
  assert(gl_endpoint);
  return *gl_endpoint;
}

/**
 *  Load singleton.
 */
void endpoint::load() {
  if (!gl_endpoint) {
    gl_endpoint = new endpoint;
    gl_loaded = true;
  }
}

/**
 * @brief Tell if the applier is loaded.
 *
 * @return a boolean.
 */
bool endpoint::loaded() {
  return gl_loaded;
}

/**
 *  Unload singleton.
 */
void endpoint::unload() {
  gl_loaded = false;
  delete gl_endpoint;
  gl_endpoint = nullptr;
}

/**
 *  Create and register an endpoint according to configuration.
 *
 *  @param[in]  cfg      Endpoint configuration.
 *  @param[in]  mux      Muxer.
 *  @param[in]  endp     Endpoint.
 *  @param[in]  l        List of endpoints.
 */
processing::failover* endpoint::_create_failover(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params,
    std::shared_ptr<multiplexing::muxer> mux,
    std::shared_ptr<io::endpoint> endp,
    std::list<config::endpoint>& l) {
  // Debug message.
  SPDLOG_LOGGER_INFO(_logger, "endpoint applier: creating new failover '{}'",
                     cfg.name);

  // Check that failover is configured.
  std::shared_ptr<processing::failover> failovr;
  if (!cfg.failovers.empty()) {
    std::string front_failover(cfg.failovers.front());
    std::list<config::endpoint>::iterator it =
        std::find_if(l.begin(), l.end(), failover_match_name(front_failover));
    if (it == l.end())
      SPDLOG_LOGGER_ERROR(
          _logger,
          "endpoint applier: could not find failover '{}' for endpoint '{}'",
          front_failover, cfg.name);
    else {
      bool is_acceptor;
      std::shared_ptr<io::endpoint> e(
          _create_endpoint(*it, global_params, is_acceptor));
      if (is_acceptor)
        throw msg_fmt(
            "endpoint applier: cannot allow acceptor '{}' as failover for "
            "endpoint '{}'",
            front_failover, cfg.name);
      failovr = std::shared_ptr<processing::failover>(
          _create_failover(*it, global_params, mux, e, l));

      // Add secondary failovers
      for (std::list<std::string>::const_iterator
               failover_it(++cfg.failovers.begin()),
           failover_end(cfg.failovers.end());
           failover_it != failover_end; ++failover_it) {
        auto it =
            std::find_if(l.begin(), l.end(), failover_match_name(*failover_it));
        if (it == l.end())
          throw msg_fmt(
              "endpoint applier: could not find secondary failover '{}' for "
              "endpoint '{}'",
              *failover_it, cfg.name);
        bool is_acceptor{false};
        std::shared_ptr<io::endpoint> endp(
            _create_endpoint(*it, global_params, is_acceptor));
        if (is_acceptor) {
          SPDLOG_LOGGER_ERROR(
              _logger,
              "endpoint applier: secondary failover '{}' is an acceptor and "
              "cannot therefore be instantiated for endpoint '{}'",
              *failover_it, cfg.name);
        }
        failovr->add_secondary_endpoint(endp);
      }
    }
  }

  // Return failover thread.
  auto fo{std::make_unique<processing::failover>(endp, mux, cfg.name)};
  fo->set_buffering_timeout(cfg.buffering_timeout);
  fo->set_retry_interval(cfg.retry_interval);
  fo->set_failover(failovr);
  return fo.release();
}

/**
 *  Create a new endpoint object.
 *
 *  @param[in]  cfg          The config.
 *  @param[out] is_acceptor  Set to true if endpoint is an acceptor.
 *
 *  @return A new endpoint.
 */
std::shared_ptr<io::endpoint> endpoint::_create_endpoint(
    config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params,
    bool& is_acceptor) {
  // Create endpoint object.
  std::shared_ptr<io::endpoint> endp;
  int level{0};
  for (std::map<std::string, io::protocols::protocol>::const_iterator
           it = io::protocols::instance().begin(),
           end = io::protocols::instance().end();
       it != end; ++it) {
    if (it->second.osi_from == 1 &&
        it->second.endpntfactry->has_endpoint(cfg, nullptr)) {
      std::shared_ptr<persistent_cache> cache;
      if (cfg.cache_enabled) {
        log_v2::logger_id log_id = log_v2::instance().get_id(it->first);
        if (log_id == log_v2::LOGGER_SIZE)
          log_id = log_v2::CORE;
        cache = std::make_shared<persistent_cache>(
            fmt::format("{}.cache.{}",
                        config::applier::state::instance().cache_dir(),
                        cfg.name),
            log_v2::instance().get(log_id));
      }

      endp =
          std::shared_ptr<io::endpoint>(it->second.endpntfactry->new_endpoint(
              cfg, global_params, is_acceptor, cache));
      SPDLOG_LOGGER_INFO(_logger, " create endpoint {} for endpoint '{}'",
                         it->first, cfg.name);
      level = it->second.osi_to + 1;
      break;
    }
  }
  if (!endp)
    throw msg_fmt("endpoint applier: no matching type found for endpoint '{}'",
                  cfg.name);

  // Create remaining objects.
  while (level <= 7) {
    // Browse protocol list.
    std::map<std::string, io::protocols::protocol>::const_iterator it(
        io::protocols::instance().begin());
    std::map<std::string, io::protocols::protocol>::const_iterator end(
        io::protocols::instance().end());
    while (it != end) {
      if ((it->second.osi_from == level) &&
          (it->second.endpntfactry->has_endpoint(cfg, nullptr))) {
        std::shared_ptr<io::endpoint> current(
            it->second.endpntfactry->new_endpoint(cfg, global_params,
                                                  is_acceptor));
        SPDLOG_LOGGER_INFO(_logger, " create endpoint {} for endpoint '{}'",
                           it->first, cfg.name);
        current->from(endp);
        endp = current;
        level = it->second.osi_to;
        break;
      }
      ++it;
    }
    if (7 == level && it == end)
      throw msg_fmt(
          "endpoint applier: no matching protocol found for endpoint '{}'",
          cfg.name);
    ++level;
  }

  return endp;
}

/**
 *  Diff the current configuration with the new configuration.
 *
 *  @param[in]  current       Current endpoints.
 *  @param[in]  new_endpoints New endpoints configuration.
 *  @param[out] to_create     Endpoints that should be created.
 *  @param[out] to_delete     Endpoints that should be deleted.
 */
void endpoint::_diff_endpoints(
    const std::map<config::endpoint, processing::endpoint*>& current,
    const std::list<config::endpoint>& new_endpoints,
    std::list<config::endpoint>& to_create,
    std::list<config::endpoint>& to_delete) {
  // Copy some lists that we will modify.
  std::list<config::endpoint> new_ep(new_endpoints);
  for (auto it = current.begin(); it != current.end(); ++it)
    to_delete.push_back(it->first);

  // Loop through new configuration.
  while (!new_ep.empty()) {
    // Find a root entry.
    auto list_it = new_ep.begin();
    while (list_it != new_ep.end() && !list_it->name.empty() &&
           std::find_if(new_ep.begin(), new_ep.end(),
                        name_match_failover(list_it->name)) != new_ep.end())
      ++list_it;
    if (list_it == new_ep.end())
      throw msg_fmt(
          "endpoint applier: error while "
          "diff'ing new and old configuration");
    std::list<config::endpoint> entries;
    entries.push_back(*list_it);
    new_ep.erase(list_it);

    // Find all subentries.
    for (auto& entry : entries) {
      // Find failovers.
      if (!entry.failovers.empty())
        for (auto& failover : entry.failovers) {
          list_it = std::find_if(new_ep.begin(), new_ep.end(),
                                 failover_match_name(failover));
          if (list_it == new_ep.end())
            SPDLOG_LOGGER_ERROR(
                _logger,
                "endpoint applier: could not find failover '{}' for endpoint "
                "'{}'",
                failover, entry.name);
          else {
            entries.push_back(*list_it);
            new_ep.erase(list_it);
          }
        }
    }

    // Try to find entry and subentries in the endpoints already running.
    auto map_it = current.find(entries.front());
    if (map_it == current.end())
      for (auto& entry : entries)
        to_create.push_back(entry);
    else
      to_delete.remove(map_it->first);
  }
}

/**
 *  Create filters from a set of categories.
 *
 *  @param[in] cfg  Endpoint configuration.
 *  @param[in] forbidden_filter  forbidden filter applied in case of default
 * filter config
 *
 *  @return Filters.
 */
multiplexing::muxer_filter endpoint::parse_filters(
    const std::set<std::string>& str_filters,
    const multiplexing::muxer_filter& forbidden_filter) {
  auto logger = log_v2::instance().get(log_v2::CONFIG);
  multiplexing::muxer_filter elements({});
  std::forward_list<fmt::string_view> applied_filters;
  auto fill_elements = [&elements, logger](const std::string& str) -> bool {
    bool retval = false;
    io::events::events_container const& tmp_elements(
        io::events::instance().get_matching_events(str));
    for (io::events::events_container::const_iterator
             it = tmp_elements.cbegin(),
             end = tmp_elements.cend();
         it != end; ++it) {
      logger->trace("endpoint applier: new filtering element: {}", it->first);
      elements.insert(it->first);
      retval = true;
    }
    return retval;
  };

  if (str_filters.size() == 1 && *str_filters.begin() == "all") {
    elements = multiplexing::muxer_filter();
    elements -= forbidden_filter;
    applied_filters.emplace_front("all");
  } else {
    for (auto& str : str_filters) {
      bool ok = false;
      try {
        ok = fill_elements(str);
      } catch (const std::exception& e) {
        logger->error("endpoint applier: '{}' is not a known category: {}", str,
                      e.what());
      }
      if (ok)
        applied_filters.emplace_front(str);
    }
    if (applied_filters.empty() && !str_filters.empty()) {
      fill_elements("all");
      elements -= forbidden_filter;
      applied_filters.emplace_front("all");
    }
  }
  SPDLOG_LOGGER_INFO(logger, "Filters applied on endpoint:{}",
                     fmt::join(applied_filters, ", "));
  return elements;
}
