/**
 * Copyright 2020-2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "broker/core/brokerrpc/broker_impl.hh"
#include <google/protobuf/util/time_util.h>
#include <grpcpp/support/status.h>

#include "broker/core/cache/broker_cache.hh"
#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/endpoint.hh"
#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/stats/helper.hh"
#include "com/centreon/broker/version.hh"
#include "com/centreon/common/process_stat.hh"
#include "common/crypto/aes256.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::version;
using com::centreon::common::crypto::aes256;
using com::centreon::common::log_v2::log_v2;

broker_impl::broker_impl() {}

/**
 * @brief Return the Broker's version.
 *
 * @param context gRPC context
 * @param  unused
 * @param response A Version object to fill
 *
 * @return Status::OK
 */
grpc::Status broker_impl::GetVersion(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const ::google::protobuf::Empty* request
                                     [[maybe_unused]],
                                     Version* response) {
  response->set_major(major);
  response->set_minor(minor);
  response->set_patch(patch);
  return grpc::Status::OK;
}

/**
 * @brief Return the number of currently loaded modules.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response A GenericSize to fill with the module count.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetNumModules(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const ::google::protobuf::Empty*,
                                        GenericSize* response) {
  auto& mod_applier(config::applier::state::instance().get_modules());

  std::lock_guard<std::mutex> lock(mod_applier.module_mutex());
  response->set_size(std::distance(mod_applier.begin(), mod_applier.end()));

  return grpc::Status::OK;
}

/**
 * @brief Return the number of currently active endpoints.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response A GenericSize to fill with the endpoint count.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetNumEndpoint(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const ::google::protobuf::Empty*,
                                         GenericSize* response) {
  // Endpoint applier.
  config::applier::endpoint& endp_applier(
      config::applier::endpoint::instance());

  std::lock_guard<std::timed_mutex> lock(endp_applier.endpoints_mutex());
  response->set_size(std::distance(endp_applier.endpoints_begin(),
                                   endp_applier.endpoints_end()));

  return grpc::Status::OK;
}

/**
 * @brief Return statistics of loaded modules as a JSON-encoded string. If a
 * name or index is specified, only that module's statistics are returned;
 * otherwise all modules are included.
 *
 * @param context gRPC context (unused).
 * @param request A GenericNameOrIndex to select a module by name or index.
 * Leaving it unset returns stats for all modules.
 * @param response A GenericString filled with the JSON-encoded statistics.
 *
 * @return grpc::Status::OK, or grpc::INVALID_ARGUMENT if the name or index
 * is not found.
 */
grpc::Status broker_impl::GetModulesStats(grpc::ServerContext* context
                                          [[maybe_unused]],
                                          const GenericNameOrIndex* request,
                                          GenericString* response) {
  std::vector<nlohmann::json> value;
  stats::get_loaded_module_stats(value);

  bool found{false};
  nlohmann::json object;
  switch (request->nameOrIndex_case()) {
    case GenericNameOrIndex::NAMEORINDEX_NOT_SET:
      for (auto& obj : value) {
        object["module" + obj["name"].get<std::string>()] = obj;
      }
      response->set_str_arg(object.dump());
      break;

    case GenericNameOrIndex::kStr:
      for (auto& obj : value) {
        if (obj["name"].get<std::string>() == request->str()) {
          found = true;
          response->set_str_arg(object.dump());
          break;
        }
      }
      if (!found)
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("name not found"));

      break;

    case GenericNameOrIndex::kIdx:

      if (request->idx() + 1 > value.size())
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("idx too big"));

      object = value[request->idx()];
      response->set_str_arg(object.dump());
      break;

    default:
      return grpc::Status::CANCELLED;
      break;
  }

  return grpc::Status::OK;
}

/**
 * @brief Return statistics of active endpoints as a JSON-encoded string. If
 * a name or index is specified, only that endpoint's statistics are returned;
 * otherwise all endpoints are included.
 *
 * @param context gRPC context (unused).
 * @param request A GenericNameOrIndex to select an endpoint by name or index.
 * Leaving it unset returns stats for all endpoints.
 * @param response A GenericString filled with the JSON-encoded statistics.
 *
 * @return grpc::Status::OK, grpc::UNAVAILABLE if the endpoint lock cannot be
 * acquired, grpc::ABORTED if the endpoint throws an exception, or
 * grpc::INVALID_ARGUMENT if the name or index is not found.
 */
grpc::Status broker_impl::GetEndpointStats(grpc::ServerContext* context
                                           [[maybe_unused]],
                                           const GenericNameOrIndex* request,
                                           GenericString* response) {
  std::vector<nlohmann::json> value;
  try {
    if (!stats::get_endpoint_stats(value))
      return grpc::Status(grpc::UNAVAILABLE, grpc::string("endpoint locked"));
  } catch (...) {
    return grpc::Status(grpc::ABORTED, grpc::string("endpoint throw error"));
  }

  bool found{false};
  nlohmann::json object;

  switch (request->nameOrIndex_case()) {
    case GenericNameOrIndex::NAMEORINDEX_NOT_SET:
      for (auto& obj : value) {
        object["module" + obj["name"].get<std::string>()] = obj;
      }
      response->set_str_arg(object.dump());
      break;

    case GenericNameOrIndex::kStr:
      for (auto& obj : value) {
        if (obj["name"].get<std::string>() == request->str()) {
          found = true;
          response->set_str_arg(obj.dump());
          break;
        }
      }
      if (!found)
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("name not found"));
      break;

    case GenericNameOrIndex::kIdx:

      if ((request->idx() + 1) > value.size())
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("idx too big"));

      object = value[request->idx()];
      response->set_str_arg(object.dump());
      break;

    default:
      return grpc::Status::CANCELLED;
      break;
  }
  return grpc::Status::OK;
}

/**
 * @brief Return generic Broker statistics as a JSON-encoded string.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response A GenericString filled with the JSON-encoded statistics.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetGenericStats(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericString* response) {
  nlohmann::json object;
  stats::get_generic_stats(object);

  response->set_str_arg(object.dump());
  return grpc::Status::OK;
}

/**
 * @brief Return SQL manager statistics. If a connection ID is specified, only
 * that connection's statistics are returned; otherwise all connections are
 * included.
 *
 * @param context gRPC context (unused).
 * @param request A SqlConnection optionally specifying a connection ID.
 * @param response A SqlManagerStats message to fill.
 *
 * @return grpc::Status::OK, or grpc::StatusCode::NOT_FOUND if the specified
 * connection ID does not exist.
 */
grpc::Status broker_impl::GetSqlManagerStats(grpc::ServerContext* context
                                             [[maybe_unused]],
                                             const SqlConnection* request,
                                             SqlManagerStats* response) {
  auto center = config::applier::state::instance().center();
  if (!request->has_id())
    center->get_sql_manager_stats(response);
  else {
    try {
      center->get_sql_manager_stats(response, request->id());
    } catch (const std::exception& e) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, e.what());
    }
  }
  return grpc::Status::OK;
}

/**
 * @brief Set options controlling SQL manager statistics collection, namely
 * the number of slowest statements and queries to track.
 *
 * @param context gRPC context (unused).
 * @param request A SqlManagerStatsOptions message with optional fields
 * slowest_statements_count and slowest_queries_count.
 * @param response Unused.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::SetSqlManagerStats(
    grpc::ServerContext* context [[maybe_unused]],
    const SqlManagerStatsOptions* request,
    ::google::protobuf::Empty*) {
  auto& conf = config::applier::state::mut_stats_conf();

  if (request->has_slowest_statements_count())
    conf.sql_slowest_statements_count = request->slowest_statements_count();
  if (request->has_slowest_queries_count())
    conf.sql_slowest_queries_count = request->slowest_queries_count();

  return grpc::Status::OK;
}

/**
 * @brief Return statistics of the conflict manager.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response A ConflictManagerStats message to fill.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetConflictManagerStats(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ConflictManagerStats* response) {
  config::applier::state::instance().center()->get_conflict_manager_stats(
      response);
  return grpc::Status::OK;
}

/**
 * @brief Return statistics for the muxer with the given name.
 *
 * @param context gRPC context (unused).
 * @param request A GenericString whose str_arg field contains the muxer name.
 * @param response A MuxerStats message to fill.
 *
 * @return grpc::Status::OK, or grpc::StatusCode::NOT_FOUND if no muxer with
 * that name exists.
 */
grpc::Status broker_impl::GetMuxerStats(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const GenericString* request,
                                        MuxerStats* response) {
  const std::string name = request->str_arg();
  bool status =
      config::applier::state::instance().center()->muxer_stats(name, response);
  return status ? grpc::Status::OK
                : grpc::Status(
                      grpc::StatusCode::NOT_FOUND,
                      fmt::format("no muxer stats found for name '{}'", name));
}

/**
 * @brief The internal part of the gRPC RebuildMetrics() function.
 *
 * @param context (unused)
 * @param request A pointer to a MetricIds which contains a vector of metric
 * ids. These ids correspond to the metrics to rebuild.
 * @param response (unused)
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::RebuildRRDGraphs(grpc::ServerContext* context
                                           [[maybe_unused]],
                                           const IndexIds* request,
                                           ::google::protobuf::Empty* response
                                           [[maybe_unused]]) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_rebuild_graphs>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

/**
 * @brief Remove RRD files for the given index and metric IDs.
 *
 * @param context gRPC context (unused).
 * @param request A ToRemove message containing vectors of index_ids and
 * metric_ids identifying the RRD files to remove.
 * @param response Unused.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::RemoveGraphs(grpc::ServerContext* context
                                       [[maybe_unused]],
                                       const ToRemove* request,
                                       ::google::protobuf::Empty* response
                                       [[maybe_unused]]) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_remove_graphs>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

/**
 * @brief Build a file with the BA content and its relations.
 *
 * @param context gRPC context (unused).
 * @param request A BaInfo message containing the BA ID and the path to the
 * output file (currently a *.dot file).
 * @param response Unused.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetBa(grpc::ServerContext* context [[maybe_unused]],
                                const BaInfo* request,
                                ::google::protobuf::Empty* response
                                [[maybe_unused]]) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<extcmd::pb_ba_info>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

/**
 * @brief Return processing statistics including engine state and per-muxer
 * statistics.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response A ProcessingStats message to fill.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetProcessingStats(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::ProcessingStats* response) {
  config::applier::state::instance().center()->get_processing_stats(response);
  return grpc::Status::OK;
}

/**
 * @brief Remove a poller configuration from Broker and the real-time
 * database.
 *
 * @param context gRPC context (unused).
 * @param request A GenericNameOrIndex containing the poller name or its ID.
 * @param response Unused.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::RemovePoller(grpc::ServerContext* context
                                       [[maybe_unused]],
                                       const GenericNameOrIndex* request,
                                       ::google::protobuf::Empty*) {
  log_v2::instance().get(log_v2::CORE)->info("Remove poller...");
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_remove_poller>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

/**
 * @brief Retrieve information about loggers. If a name is specified, only
 * that logger's level is returned; otherwise all loggers are included.
 *
 * @param context gRPC context (unused).
 * @param request A GenericString whose str_arg contains a logger name, or
 * empty to retrieve all loggers.
 * @param response A LogInfo message with the log name, log file, flush
 * period, and a map of logger names to their current levels.
 *
 * @return grpc::Status::OK, or grpc::StatusCode::INVALID_ARGUMENT if the
 * given logger name does not exist.
 */
grpc::Status broker_impl::GetLogInfo(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const GenericString* request,
                                     LogInfo* response) {
  auto& name{request->str_arg()};
  auto& map = *response->mutable_level();
  auto lvs = log_v2::instance().levels();
  response->set_log_name(log_v2::instance().log_name());
  response->set_log_file(log_v2::instance().filename());
  response->set_log_flush_period(log_v2::instance().flush_interval().count());
  if (!name.empty()) {
    auto found = std::find_if(
        lvs.begin(), lvs.end(),
        [&name](std::pair<std::string, spdlog::level::level_enum>& p) {
          return p.first == name;
        });
    if (found != lvs.end()) {
      auto level = to_string_view(found->second);
      map[name] = std::string(level.data(), level.size());
      return grpc::Status::OK;
    } else {
      std::string msg{fmt::format("'{}' is not a logger in broker", name)};
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, msg);
    }
  } else {
    for (auto& p : lvs) {
      auto level = to_string_view(p.second);
      map[p.first] = std::string(level.data(), level.size());
    }
    return grpc::Status::OK;
  }
}

/**
 * @brief Set the log level of a specific logger.
 *
 * @param context gRPC context (unused).
 * @param request A LogLevel message containing the logger name and the
 * desired level.
 * @param response Unused.
 *
 * @return grpc::Status::OK, or grpc::StatusCode::INVALID_ARGUMENT if the
 * logger does not exist.
 */
grpc::Status broker_impl::SetLogLevel(grpc::ServerContext* context
                                      [[maybe_unused]],
                                      const LogLevel* request,
                                      ::google::protobuf::Empty*) {
  const std::string& logger_name{request->logger()};
  std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
  if (!logger) {
    std::string err_detail =
        fmt::format("The '{}' logger does not exist", logger_name);
    SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::CORE), err_detail);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err_detail);
  } else {
    logger->set_level(spdlog::level::level_enum(request->level()));
    return grpc::Status::OK;
  }
}

/**
 * @brief Set the flush period for all loggers.
 *
 * @param context gRPC context (unused).
 * @param request A LogFlushPeriod message containing the period in seconds.
 * A value of 0 means flush after every log entry.
 * @param response Unused.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::SetLogFlushPeriod(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const LogFlushPeriod* request,
                                            ::google::protobuf::Empty*) {
  log_v2::instance().set_flush_interval(request->period());
  return grpc::Status::OK;
}

/**
 * @brief get stats of the process (cpu, memory...)
 *
 * @param context
 * @param request
 * @param response
 * @return ::grpc::Status
 */
::grpc::Status broker_impl::GetProcessStats(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::com::centreon::common::pb_process_stat* response) {
  try {
    com::centreon::common::process_stat stat(getpid());
    stat.to_protobuff(*response);
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::CORE),
                        "fail to get process info: {}",
                        boost::diagnostic_information(e));

    return grpc::Status(grpc::StatusCode::INTERNAL,
                        boost::diagnostic_information(e));
  }
  return grpc::Status::OK;
}

/**
 * @brief Encrypt a string using AES-256.
 *
 * @param context gRPC context (unused).
 * @param request An AesMessage containing the app_secret (key), salt, and
 * content to encrypt.
 * @param response A GenericString filled with the encrypted result.
 *
 * @return grpc::Status::OK, or grpc::INVALID_ARGUMENT if encryption fails.
 */
grpc::Status broker_impl::Aes256Encrypt(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const AesMessage* request,
                                        GenericString* response) {
  std::string first_key = request->app_secret();
  std::string second_key = request->salt();

  try {
    aes256 access(first_key, second_key);
    std::string result = access.encrypt(request->content());
    response->set_str_arg(result);
    return grpc::Status::OK;
  } catch (const std::exception& e) {
    return grpc::Status(grpc::INVALID_ARGUMENT, grpc::string(e.what()));
  }
}

/**
 * @brief Decrypt a string using AES-256.
 *
 * @param context gRPC context (unused).
 * @param request An AesMessage containing the app_secret (key), salt, and
 * content to decrypt.
 * @param response A GenericString filled with the decrypted result.
 *
 * @return grpc::Status::OK, or grpc::INVALID_ARGUMENT if decryption fails.
 */
grpc::Status broker_impl::Aes256Decrypt(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const AesMessage* request,
                                        GenericString* response) {
  std::string first_key = request->app_secret();
  std::string second_key = request->salt();

  try {
    aes256 access(first_key, second_key);
    std::string result = access.decrypt(request->content());
    response->set_str_arg(result);
    return grpc::Status::OK;
  } catch (const std::exception& e) {
    return grpc::Status(grpc::INVALID_ARGUMENT, grpc::string(e.what()));
  }
}

/**
 * @brief Return the list of peers currently connected to this Broker instance.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response A PeerList message populated with one Peer entry per
 * connected peer.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetPeers(grpc::ServerContext* context
                                   [[maybe_unused]],
                                   const ::google::protobuf::Empty* request
                                   [[maybe_unused]],
                                   PeerList* response) {
  /* The Broker gRPC service is only available on Broker instances
   * with the Broker role. So it's safe to static_cast the state to
   * broker_state.
   */
  config::applier::broker_state* broker_state =
      static_cast<config::applier::broker_state*>(
          &config::applier::state::instance());
  if (broker_state) {
    for (auto& p : broker_state->connected_peers()) {
      auto peer = response->add_peers();
      peer->set_id(p.poller_id);
      peer->set_poller_name(p.poller_name);
      peer->set_broker_name(p.broker_name);
      peer->mutable_connected_since()->set_seconds(p.connected_since);
      peer->set_engine_conf(p.engine_conf);
      peer->set_available_conf(p.available_conf);
      peer->set_type(p.peer_type);
    }
  }
  return grpc::Status::OK;
}

/**
 * @brief Return the IDs of all hosts currently in the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response An IdsList populated with all host IDs.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetHostIds(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const ::google::protobuf::Empty* request
                                     [[maybe_unused]],
                                     IdsList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_HOSTS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Host cache is not enabled in this broker instance");
  auto lst = cache.host_ids();
  response->mutable_ids()->Reserve(lst.size());
  for (uint64_t host_id : lst)
    response->add_ids(host_id);
  return grpc::Status::OK;
}

/**
 * @brief Return a host from the broker cache, looked up by name or ID.
 *
 * @param context gRPC context (unused).
 * @param request A GenericNameOrIndex: use str for name-based lookup, idx
 * for ID-based lookup.
 * @param response A Host message filled with the matching host's data.
 *
 * @return grpc::Status::OK, grpc::StatusCode::NOT_FOUND if the host is not
 * in the cache, or grpc::StatusCode::INVALID_ARGUMENT if neither name nor
 * index is set.
 */
grpc::Status broker_impl::GetHost(grpc::ServerContext* context [[maybe_unused]],
                                  const GenericNameOrIndex* request,
                                  Host* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_HOSTS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Host cache is not enabled in this broker instance");
  switch (request->nameOrIndex_case()) {
    case GenericNameOrIndex::kStr: {
      auto const& host = cache.host(request->str());
      if (!host) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND,
                            fmt::format("Host '{}' not found", request->str()));
      }
      response->CopyFrom(host->obj());
    } break;
    case GenericNameOrIndex::kIdx: {
      auto host = cache.host(request->idx());
      if (!host) {
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            fmt::format("Host with id '{}' not found", request->idx()));
      } else
        response->CopyFrom(host->obj());
    } break;
    case GenericNameOrIndex::NAMEORINDEX_NOT_SET:
    default:
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "Either name or index must be set");
  }
  return grpc::Status::OK;
}

/**
 * @brief Return the (host_id, service_id) pairs of all services currently in
 * the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response An IdsPairsList populated with all (host_id, service_id)
 * pairs.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetServiceIds(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const ::google::protobuf::Empty* request
                                        [[maybe_unused]],
                                        IdsPairsList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_SERVICES))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Service cache is not enabled in this broker instance");
  auto lst = cache.service_ids();
  for (const auto& [host_id, service_id] : lst) {
    auto* pair = response->add_pairs();
    pair->set_host_id(host_id);
    pair->set_service_id(service_id);
  }
  return grpc::Status::OK;
}

/**
 * @brief Return a service from the broker cache, looked up by host_id and
 * service_id.
 *
 * @param context gRPC context (unused).
 * @param request A ServiceIdentifier with the host_id and service_id fields
 * set.
 * @param response A Service message filled with the matching service's data.
 *
 * @return grpc::Status::OK, grpc::StatusCode::NOT_FOUND if the service is
 * not in the cache, or grpc::StatusCode::INVALID_ARGUMENT if the IDs are
 * not provided.
 */
grpc::Status broker_impl::GetService(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const ServiceIdentifier* request,
                                     com::centreon::broker::Service* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_SERVICES))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Service cache is not enabled in this broker instance");
  uint64_t host_id = std::numeric_limits<uint64_t>::max();
  uint64_t service_id = std::numeric_limits<uint64_t>::max();
  switch (request->host_case()) {
    case ServiceIdentifier::kHostName: {
    } break;
    case ServiceIdentifier::kHostId: {
      host_id = request->host_id();
    } break;
  }
  switch (request->service_case()) {
    case ServiceIdentifier::kDescription: {
    } break;
    case ServiceIdentifier::kServiceId: {
      service_id = request->service_id();
    } break;
  }

  if (host_id == std::numeric_limits<uint64_t>::max() ||
      service_id == std::numeric_limits<uint64_t>::max()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Both host_id and service_id must be set");
  }
  auto service = cache.service(host_id, service_id);
  if (!service)
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        fmt::format("Service with id '{}:{}' not found", host_id, service_id));
  else
    response->CopyFrom(service->obj());
  return grpc::Status::OK;
}

/**
 * @brief Return the IDs of all hostgroups currently in the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response An IdsList populated with all hostgroup IDs.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetHostGroupIds(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    IdsList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_GROUPS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Group cache is not enabled in this broker instance");
  auto lst = cache.hostgroup_ids();
  response->mutable_ids()->Reserve(lst.size());
  for (uint64_t hg_id : lst)
    response->add_ids(hg_id);
  return grpc::Status::OK;
}

/**
 * @brief Return a hostgroup from the broker cache with its member host IDs,
 * looked up by name or ID.
 *
 * @param context gRPC context (unused).
 * @param request A GenericNameOrIndex: use str for name-based lookup, idx
 * for ID-based lookup.
 * @param response A HostGroup message with the member_host_ids field
 * populated.
 *
 * @return grpc::Status::OK, grpc::StatusCode::NOT_FOUND if the hostgroup is
 * not in the cache, or grpc::StatusCode::INVALID_ARGUMENT if neither name
 * nor index is set.
 */
grpc::Status broker_impl::GetHostGroup(grpc::ServerContext* context
                                       [[maybe_unused]],
                                       const GenericNameOrIndex* request,
                                       HostGroup* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_GROUPS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Group cache is not enabled in this broker instance");
  std::shared_ptr<neb::pb_host_group> hg;
  switch (request->nameOrIndex_case()) {
    case GenericNameOrIndex::kStr:
      hg = cache.hostgroup(request->str());
      if (!hg)
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            fmt::format("Hostgroup '{}' not found", request->str()));
      break;
    case GenericNameOrIndex::kIdx:
      hg = cache.hostgroup(request->idx());
      if (!hg)
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            fmt::format("Hostgroup with id '{}' not found", request->idx()));
      break;
    case GenericNameOrIndex::NAMEORINDEX_NOT_SET:
    default:
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "Either name or index must be set");
  }
  response->CopyFrom(hg->obj());
  auto members = cache.hostgroup_members(hg->obj().hostgroup_id());
  response->mutable_member_host_ids()->Reserve(members.size());
  for (uint64_t host_id : members)
    response->add_member_host_ids(host_id);
  return grpc::Status::OK;
}

/**
 * @brief Return the IDs of all servicegroups currently in the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response An IdsList populated with all servicegroup IDs.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetServiceGroupIds(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    IdsList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_GROUPS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Group cache is not enabled in this broker instance");
  auto lst = cache.servicegroup_ids();
  response->mutable_ids()->Reserve(lst.size());
  for (uint64_t sg_id : lst)
    response->add_ids(sg_id);
  return grpc::Status::OK;
}

/**
 * @brief Return a servicegroup from the broker cache with its member
 * (host_id, service_id) pairs, looked up by ID.
 *
 * @param context gRPC context (unused).
 * @param request A GenericNameOrIndex: use idx for ID-based lookup.
 * @param response A ServiceGroup message with the member_service_ids field
 * populated.
 *
 * @return grpc::Status::OK, or grpc::StatusCode::INVALID_ARGUMENT if the
 * index is not set.
 */
grpc::Status broker_impl::GetServiceGroup(grpc::ServerContext* context
                                          [[maybe_unused]],
                                          const GenericNameOrIndex* request,
                                          ServiceGroup* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_GROUPS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Group cache is not enabled in this broker instance");
  std::shared_ptr<neb::pb_service_group> sg;
  switch (request->nameOrIndex_case()) {
    case GenericNameOrIndex::kIdx:
      sg = cache.servicegroup(request->idx());
      if (!sg)
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            fmt::format("Servicegroup with id '{}' not found", request->idx()));
      break;
    case GenericNameOrIndex::kStr:
    case GenericNameOrIndex::NAMEORINDEX_NOT_SET:
    default:
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "Servicegroup index must be set");
  }
  response->CopyFrom(sg->obj());
  auto members = cache.servicegroup_members(sg->obj().servicegroup_id());
  response->mutable_member_service_ids()->Reserve(members.size());
  for (const auto& [host_id, service_id] : members) {
    auto* m = response->add_member_service_ids();
    m->set_host_id(host_id);
    m->set_service_id(service_id);
  }
  return grpc::Status::OK;
}

/**
 * @brief Return all severities currently held in the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request Empty request.
 * @param response A SeverityList populated with one SeverityEntry per cached
 * severity.
 *
 * @return grpc::Status::OK.
 */
grpc::Status broker_impl::GetSeverities(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const ::google::protobuf::Empty* request
                                        [[maybe_unused]],
                                        SeverityList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_SEVERITIES))
    return grpc::Status(
        grpc::StatusCode::UNAVAILABLE,
        "Severity cache is not enabled in this broker instance");
  auto sevs = cache.severities();
  response->mutable_entries()->Reserve(sevs.size());
  for (const auto& [key, sev] : sevs) {
    auto* entry = response->add_entries();
    entry->set_config_id(key.first);
    entry->set_type(static_cast<Severity::Type>(key.second));
    entry->set_level(sev.level);
    entry->set_db_id(sev.db_id);
  }
  return grpc::Status::OK;
}
