/**
 * Copyright 2020-2026 Centreon (https://www.centreon.com/)
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
#include <absl/strings/ascii.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/support/status.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <algorithm>
#include "common/downtimes/downtime_manager.hh"
#include "common/engine_conf/parser.hh"

#include "broker/core/bbdo/internal.hh"
#include "broker/core/config/applier/broker_state.hh"
#include "broker/core/config/applier/endpoint.hh"
#include "com/centreon/broker/broker_acknowledgement_manager.hh"
#include "com/centreon/broker/broker_comments.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/stats/helper.hh"
#include "com/centreon/broker/version.hh"
#include "com/centreon/common/process_stat.hh"
#include "common/crypto/aes256.hh"
#include "common/external_commands/command_table.hh"
#include "common/notifications/notification_manager.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::version;
using com::centreon::common::crypto::aes256;
using com::centreon::common::downtimes::downtime;
using com::centreon::common::downtimes::downtime_manager;
using com::centreon::common::log_v2::log_v2;

namespace {
/**
 * @brief A spdlog sink that keeps the log records in memory instead of writing
 * them anywhere.
 *
 * Used by CheckPollerConfig to collect the warnings/errors that the config
 * validation logs. It is attached to a dedicated, single-threaded logger (only
 * the RPC handler thread uses it), so it needs no locking (null_mutex) and
 * never touches broker's shared loggers.
 */
class capturing_sink
    : public spdlog::sinks::base_sink<spdlog::details::null_mutex> {
 public:
  struct record {
    spdlog::level::level_enum level;
    std::string message;
  };
  std::vector<record> records;

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    records.push_back(
        {msg.level, std::string(msg.payload.data(), msg.payload.size())});
  }
  void flush_() override {}
};

/**
 * @brief Guard of the Broker-owned features (downtimes, comments,
 * acknowledgements, notification switches...), available only in
 * notification_mode = broker.
 *
 * @param loaded  Whether the manager owning the feature is loaded.
 * @param feature The feature name for the error message ("Downtime"...).
 *
 * @return The UNAVAILABLE status to return when @p loaded is false,
 * std::nullopt otherwise.
 */
std::optional<grpc::Status> unavailable_unless(bool loaded,
                                               std::string_view feature) {
  if (loaded)
    return std::nullopt;
  return grpc::Status(
      grpc::StatusCode::UNAVAILABLE,
      fmt::format("{} management is not enabled (notification_mode != broker)",
                  feature));
}

/**
 * @brief Resolve a host from a HostIdentifier (name or id) in the Broker
 * cache. Shared by every RPC that designates a host this way.
 *
 * @param id     The identifier.
 * @param status Set to the gRPC error when the host cannot be resolved.
 *
 * @return The cached host, or nullptr (status then tells why).
 */
std::shared_ptr<neb::pb_host> resolve_host(const HostIdentifier& id,
                                           grpc::Status* status) {
  auto& cache = config::applier::state::instance().cache();
  std::shared_ptr<neb::pb_host> h;
  switch (id.host_case()) {
    case HostIdentifier::kHostName:
      h = cache.host(id.host_name());
      break;
    case HostIdentifier::kHostId:
      h = cache.host(id.host_id());
      break;
    default:
      *status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                             "host_name or host_id must be set");
      return nullptr;
  }
  if (!h)
    *status = grpc::Status(grpc::StatusCode::NOT_FOUND, "could not find host");
  return h;
}

/**
 * @brief Resolve a poller id from a PollerIdentifier (id or name) in the
 * Broker cache. Shared by every RPC that designates a poller this way.
 *
 * @param id     The identifier.
 * @param status Set to the gRPC error when the poller cannot be resolved.
 *
 * @return The poller id, or 0 (status then tells why).
 */
uint64_t resolve_poller(const PollerIdentifier& id, grpc::Status* status) {
  switch (id.poller_case()) {
    case PollerIdentifier::kPollerId:
      return id.poller_id();
    case PollerIdentifier::kPollerName: {
      auto pid = config::applier::state::instance().cache().instance_id(
          id.poller_name());
      if (!pid) {
        *status =
            grpc::Status(grpc::StatusCode::NOT_FOUND,
                         fmt::format("unknown poller '{}'", id.poller_name()));
        return 0;
      }
      return *pid;
    }
    default:
      *status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                             "poller_id or poller_name must be set");
      return 0;
  }
}

/**
 * @brief Build a pb_external_command addressed to a poller.
 *
 * @param poller_id  The destination poller.
 * @param line       The full legacy line, "[timestamp] NAME;args".
 * @param host_id    The host the command targets, 0 if none.
 * @param service_id The service the command targets, 0 if none.
 *
 * @return The event, ready for broker_state::push_pending_for_poller().
 */
std::shared_ptr<bbdo::pb_external_command> make_external_command(
    uint64_t poller_id,
    const std::string& line,
    uint64_t host_id = 0,
    uint64_t service_id = 0) {
  auto evt = std::make_shared<bbdo::pb_external_command>();
  evt->source_id = 0;
  evt->destination_id = static_cast<uint32_t>(poller_id);
  auto& obj = evt->mut_obj();
  obj.set_command(line);
  obj.set_host_id(host_id);
  obj.set_service_id(service_id);
  return evt;
}

/**
 * @brief Resolve a service from a ServiceIdentifier (host by name or id,
 * service by description or id) in the Broker cache.
 *
 * @param id     The identifier.
 * @param status Set to the gRPC error when the service cannot be resolved.
 *
 * @return The cached service, or nullptr (status then tells why).
 */
std::shared_ptr<neb::pb_service> resolve_service(const ServiceIdentifier& id,
                                                 grpc::Status* status) {
  auto& cache = config::applier::state::instance().cache();
  std::shared_ptr<neb::pb_host> h;
  switch (id.host_case()) {
    case ServiceIdentifier::kHostName:
      h = cache.host(id.host_name());
      break;
    case ServiceIdentifier::kHostId:
      h = cache.host(id.host_id());
      break;
    default:
      *status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                             "host_name or host_id must be set");
      return nullptr;
  }
  if (!h) {
    *status = grpc::Status(grpc::StatusCode::NOT_FOUND, "could not find host");
    return nullptr;
  }
  std::shared_ptr<neb::pb_service> s;
  switch (id.service_case()) {
    case ServiceIdentifier::kDescription:
      s = cache.service(h->obj().name(), id.description());
      break;
    case ServiceIdentifier::kServiceId:
      s = cache.service(h->obj().host_id(), id.service_id());
      break;
    default:
      *status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                             "description or service_id must be set");
      return nullptr;
  }
  if (!s)
    *status =
        grpc::Status(grpc::StatusCode::NOT_FOUND, "could not find service");
  return s;
}

}  // namespace

broker_impl::broker_impl() : _logger{log_v2::instance().get(log_v2::CORE)} {}

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
  _logger->info("Remove poller...");
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
    SPDLOG_LOGGER_ERROR(_logger, err_detail);
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
    SPDLOG_LOGGER_ERROR(_logger, "fail to get process info: {}",
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
 * @brief Return the list of pollers currently connected to this Broker
 * instance.
 *
 * @param context gRPC context (unused).
 * @param request Unused.
 * @param response A PeerList message populated with one Peer entry per
 * connected peer.
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::GetPollers(grpc::ServerContext* context
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
  for (auto& p : broker_state->connected_pollers()) {
    auto peer = response->add_peers();
    peer->set_id(p.poller_id);
    peer->set_poller_name(p.poller_name);
    peer->mutable_connected_since()->set_seconds(*p.connected_since);
    peer->set_engine_conf(p.engine_conf);
    peer->set_available_conf(p.available_conf);
    peer->set_type(common::ENGINE);
    peer->set_timezone(p.timezone);
  }
  return grpc::Status::OK;
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
  for (auto& p : broker_state->connected_peers()) {
    auto peer = response->add_peers();
    peer->set_id(p.poller_id);
    peer->set_poller_name(p.poller_name);
    peer->set_broker_name(p.broker_name);
    peer->mutable_connected_since()->set_seconds(p.connected_since);
    peer->set_engine_conf(p.engine_conf);
    peer->set_available_conf(p.available_conf);
    peer->set_type(p.peer_type);
    peer->set_timezone(p.timezone);
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
  grpc::Status status;
  auto service = resolve_service(*request, &status);
  if (!service)
    return status;
  response->CopyFrom(service->obj());
  return grpc::Status::OK;
}

/**
 * @brief Evaluate the notification dependencies of a host or service as the
 * Broker sees them in its cache.
 *
 * The dependent resource is designated by the ServiceIdentifier: host by name
 * or id (required), service by name or id (optional; unset ⇒ host-level
 * check). Names are resolved to ids through the cache, then the evaluation is
 * delegated to broker_cache::notification_authorized_by_dependencies.
 *
 * @param context gRPC context (unused).
 * @param request The dependent resource identifier.
 * @param response Its authorized flag tells whether a notification is allowed.
 *
 * @return grpc::Status::OK, grpc::StatusCode::NOT_FOUND if the resource is not
 * in the cache, grpc::StatusCode::INVALID_ARGUMENT if the host is unset, or
 * grpc::StatusCode::UNAVAILABLE if the required cache section is disabled.
 */
grpc::Status broker_impl::NotificationAuthorizedByDependencies(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceIdentifier* request,
    NotificationAuthorizedByDependenciesResponse* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_HOSTS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Host cache is not enabled in this broker instance");

  /* Resolve the dependent host (required). */
  std::shared_ptr<neb::pb_host> host;
  switch (request->host_case()) {
    case ServiceIdentifier::kHostName:
      host = cache.host(request->host_name());
      if (!host)
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            fmt::format("Host '{}' not found", request->host_name()));
      break;
    case ServiceIdentifier::kHostId:
      host = cache.host(request->host_id());
      if (!host)
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            fmt::format("Host with id '{}' not found", request->host_id()));
      break;
    case ServiceIdentifier::HOST_NOT_SET:
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "Host must be specified by its ID or by its name");
  }
  uint64_t host_id = host->obj().host_id();
  uint64_t service_id = 0;

  /* Resolve the dependent service if one was given; a host-level check keeps
   * service_id at 0. */
  switch (request->service_case()) {
    case ServiceIdentifier::kDescription:
    case ServiceIdentifier::kServiceId: {
      if (!cache.section_enabled(cache::broker_cache::CACHE_SERVICES))
        return grpc::Status(
            grpc::StatusCode::UNAVAILABLE,
            "Service cache is not enabled in this broker instance");
      std::shared_ptr<neb::pb_service> service;
      if (request->service_case() == ServiceIdentifier::kDescription) {
        service = cache.service(host->obj().name(), request->description());
        if (!service)
          return grpc::Status(
              grpc::StatusCode::NOT_FOUND,
              fmt::format("Service '{}/{}' not found", host->obj().name(),
                          request->description()));
      } else {
        service = cache.service(host_id, request->service_id());
        if (!service)
          return grpc::Status(grpc::StatusCode::NOT_FOUND,
                              fmt::format("Service with id '{}:{}' not found",
                                          host_id, request->service_id()));
      }
      service_id = service->obj().service_id();
    } break;
    case ServiceIdentifier::SERVICE_NOT_SET:
      break;
  }

  response->set_authorized(
      cache.notification_authorized_by_dependencies(host_id, service_id));
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

/**
 * @brief Return all hosts that carry a given tag in the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request A TagIdentifier with the tag name and type.
 * @param response A HostList populated with one Host entry per matching host.
 *
 * @return grpc::Status::OK, or UNAVAILABLE if the host/tag cache is disabled.
 */
grpc::Status broker_impl::GetHostsByTag(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const TagIdentifier* request,
                                        HostList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_HOSTS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Host cache is not enabled in this broker instance");
  TagType tag_type = request->type();
  const std::string& tag_name = request->name();
  for (uint64_t host_id : cache.host_ids()) {
    auto names = cache.host_tag_names(host_id, tag_type);
    if (std::find(names.begin(), names.end(), tag_name) != names.end()) {
      auto host = cache.host(host_id);
      if (host)
        response->add_hosts()->CopyFrom(host->obj());
    }
  }
  return grpc::Status::OK;
}

/**
 * @brief Return all services that carry a given tag in the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request A TagIdentifier with the tag name and type.
 * @param response A ServiceList populated with one Service entry per matching
 * service.
 *
 * @return grpc::Status::OK, or UNAVAILABLE if the service/tag cache is
 * disabled.
 */
grpc::Status broker_impl::GetServicesByTag(grpc::ServerContext* context
                                           [[maybe_unused]],
                                           const TagIdentifier* request,
                                           ServiceList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_SERVICES))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Service cache is not enabled in this broker instance");
  TagType tag_type = request->type();
  const std::string& tag_name = request->name();
  for (const auto& [host_id, service_id] : cache.service_ids()) {
    auto names = cache.service_tag_names(host_id, service_id, tag_type);
    if (std::find(names.begin(), names.end(), tag_name) != names.end()) {
      auto svc = cache.service(host_id, service_id);
      if (svc)
        response->add_services()->CopyFrom(svc->obj());
    }
  }
  return grpc::Status::OK;
}

/**
 * @brief Return all tags currently held in the broker cache.
 *
 * @param context gRPC context (unused).
 * @param request Empty.
 * @param response A TagList with one TagEntry per cached tag. Each entry
 * carries the tag ID, type, name and the IDs of pollers that reference it.
 *
 * @return grpc::Status::OK, or UNAVAILABLE if the tag cache is disabled.
 */
grpc::Status broker_impl::GetTags(grpc::ServerContext* context [[maybe_unused]],
                                  const ::google::protobuf::Empty* request
                                  [[maybe_unused]],
                                  TagList* response) {
  auto& cache = config::applier::state::instance().cache();
  if (!cache.section_enabled(cache::broker_cache::CACHE_TAGS))
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Tag cache is not enabled in this broker instance");
  auto tag_map = cache.tags();
  response->mutable_entries()->Reserve(tag_map.size());
  for (const auto& [key, val] : tag_map) {
    auto* entry = response->add_entries();
    entry->set_id(key.first);
    entry->set_type(key.second);
    entry->set_name(val.first->obj().name());
    for (uint64_t pid : val.second)
      entry->add_poller_ids(pid);
  }
  return grpc::Status::OK;
}

/**
 * @brief gRPC handler: list the acknowledgements held in the Broker cache.
 *
 * @param context The gRPC server context (unused).
 * @param request An empty request (unused).
 * @param response An AcknowledgementList filled with one Acknowledgement per
 * cached acknowledgement (host acks carry service_id 0).
 * @return grpc::Status::OK, or UNAVAILABLE when neither the host nor the
 * service cache section is enabled.
 */
grpc::Status broker_impl::GetAcknowledgements(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    AcknowledgementList* response) {
  auto& cache = config::applier::state::instance().cache();
  /* Acks are stored under CACHE_HOSTS (host acks) and CACHE_SERVICES (service
   * acks); if neither is enabled the map is always empty. */
  if (!cache.section_enabled(cache::broker_cache::CACHE_HOSTS) &&
      !cache.section_enabled(cache::broker_cache::CACHE_SERVICES))
    return grpc::Status(
        grpc::StatusCode::UNAVAILABLE,
        "Acknowledgement cache is not enabled in this broker instance");
  auto acks = cache.acknowledgements();
  response->mutable_entries()->Reserve(acks.size());
  for (const auto& ack : acks)
    response->add_entries()->CopyFrom(ack->obj());
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetTopology(grpc::ServerContext* context
                                      [[maybe_unused]],
                                      const ::google::protobuf::Empty* request
                                      [[maybe_unused]],
                                      TopologyResponse* response) {
  /* The Broker gRPC service is only available on Broker instances
   * with the Broker role. So it's safe to static_cast the state to
   * broker_state.
   */
  config::applier::broker_state* broker_state =
      static_cast<config::applier::broker_state*>(
          &config::applier::state::instance());
  for (auto& p : broker_state->connected_peers()) {
    switch (p.peer_type) {
      case common::BROKER: {
        auto* entry = response->add_direct_brokers();
        entry->set_poller_id(p.poller_id);
        entry->set_broker_name(p.broker_name);
      } break;
      case common::ENGINE: {
        uint64_t via_remote = p.via_remote;
        if (via_remote) {
          auto remote =
              std::find_if(response->mutable_direct_brokers()->begin(),
                           response->mutable_direct_brokers()->end(),
                           [via_remote](const auto& b) {
                             return b.poller_id() == via_remote;
                           });
          if (remote != response->mutable_direct_brokers()->end()) {
            auto* poller = remote->add_pollers();
            poller->set_poller_id(p.poller_id);
            poller->set_poller_name(p.poller_name);
            break;
          }
        }
        auto* poller = response->add_direct_pollers();
        poller->set_poller_id(p.poller_id);
        poller->set_poller_name(p.poller_name);
      } break;
      default:
        break;
    }
  }
  return grpc::Status::OK;
}

/**
 * @brief Schedule a host or service downtime via the Broker gRPC API.
 *
 * Only available when notification_mode = broker is set in the Broker
 * configuration, which loads the downtime_manager singleton. Returns
 * UNAVAILABLE if the manager is not loaded.
 *
 * The host can be identified by name (host_name) or ID (host_id). For service
 * downtimes, the service can likewise be identified by description or ID. When
 * a name is provided the Broker cache resolves it to the numeric ID; NOT_FOUND
 * is returned if the name is unknown.
 *
 * Scheduling a HOST downtime also creates triggered SERVICE downtimes for all
 * services of that host, matching the behaviour of Engine's
 * SCHEDULE_HOST_DOWNTIME command.
 *
 * @param context  gRPC server context (unused).
 * @param request  A ScheduleDowntimeRequest message.
 * @param response A ScheduleDowntimeResponse filled with the new downtime ID.
 *
 * @return grpc::Status::OK on success, UNAVAILABLE if the downtime manager is
 *         not loaded, NOT_FOUND if the host or service name is unknown,
 *         INVALID_ARGUMENT if required fields are missing or the time window
 *         is invalid.
 */
grpc::Status broker_impl::ScheduleDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeRequest* request,
    ScheduleDowntimeResponse* response) {
  if (auto err = unavailable_unless(downtime_manager::is_loaded(), "Downtime"))
    return *err;

  // Resolve host identifier
  auto& cache = config::applier::state::instance().cache();
  uint64_t resolved_host_id = 0;
  if (request->host_case() == ScheduleDowntimeRequest::kHostId) {
    resolved_host_id = request->host_id();
  } else if (request->host_case() == ScheduleDowntimeRequest::kHostName) {
    auto h = cache.host(request->host_name());
    if (!h)
      return grpc::Status(
          grpc::StatusCode::NOT_FOUND,
          fmt::format("Host '{}' not found", request->host_name()));
    resolved_host_id = h->obj().host_id();
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name or host_id must be set");
  }

  // Resolve service identifier (only for SERVICE type)
  uint64_t resolved_service_id = 0;
  if (request->type() == ScheduleDowntimeRequest::SERVICE) {
    if (request->service_case() == ScheduleDowntimeRequest::kServiceId) {
      resolved_service_id = request->service_id();
    } else if (request->service_case() ==
               ScheduleDowntimeRequest::kServiceDescription) {
      auto s = cache.service(request->host_name().empty()
                                 ? cache.host(resolved_host_id)->obj().name()
                                 : request->host_name(),
                             request->service_description());
      if (!s)
        return grpc::Status(
            grpc::StatusCode::NOT_FOUND,
            fmt::format("Service '{}/{}' not found", resolved_host_id,
                        request->service_description()));
      resolved_service_id = s->obj().service_id();
    } else {
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "service_description or service_id must be set for "
                          "SERVICE downtime");
    }
  }

  downtime::type dt_type = request->type() == ScheduleDowntimeRequest::HOST
                               ? downtime::host_downtime
                               : downtime::service_downtime;

  uint64_t new_id = 0;
  bool ok = downtime_manager::instance().schedule_downtime(
      dt_type, resolved_host_id, resolved_service_id,
      static_cast<time_t>(request->entry_time()), request->author(),
      request->comment_data(), static_cast<time_t>(request->start_time()),
      static_cast<time_t>(request->end_time()), request->fixed(),
      request->triggered_by(), request->duration(), &new_id);

  if (!ok)
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid downtime parameters (end_time <= now or "
                        "end_time <= start_time)");

  if (request->type() == ScheduleDowntimeRequest::HOST) {
    for (uint64_t svc_id : cache.service_ids_for_host(resolved_host_id)) {
      uint64_t svc_downtime_id;
      downtime_manager::instance().schedule_downtime(
          downtime::service_downtime, resolved_host_id, svc_id,
          static_cast<time_t>(request->entry_time()), request->author(),
          request->comment_data(), static_cast<time_t>(request->start_time()),
          static_cast<time_t>(request->end_time()), request->fixed(), new_id,
          request->duration(), &svc_downtime_id);
    }
  }

  response->set_downtime_id(new_id);
  return grpc::Status::OK;
}

/**
 * @brief Cancel (delete) a previously scheduled downtime by its ID.
 *
 * Only available when notification_mode = broker is set. The downtime ID must
 * be passed as the idx field of a GenericNameOrIndex message. Cancelling a
 * host downtime cascades to all triggered service downtimes that were created
 * alongside it.
 *
 * @param context  gRPC server context (unused).
 * @param request  A GenericNameOrIndex with idx set to the downtime ID.
 * @param response Empty.
 *
 * @return grpc::Status::OK on success, UNAVAILABLE if the downtime manager is
 *         not loaded, INVALID_ARGUMENT if idx is not provided, NOT_FOUND if
 *         no downtime with the given ID exists.
 */
grpc::Status broker_impl::DeleteDowntime(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const DowntimeIdentifier* request,
                                         ::google::protobuf::Empty* response
                                         [[maybe_unused]]) {
  if (auto err = unavailable_unless(downtime_manager::is_loaded(), "Downtime"))
    return *err;

  if (request->downtime_id() == 0)
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "downtime_id must be set");

  bool ok =
      downtime_manager::instance().unschedule_downtime(request->downtime_id());
  if (!ok)
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        fmt::format("downtime {} not found", request->downtime_id()));

  return grpc::Status::OK;
}

/**
 * @brief Add a user comment on a host (notification_mode = broker): publish a
 * USER / EXTERNAL comment and return its internal_id.
 */
grpc::Status broker_impl::AddHostComment(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const HostCommentRequest* request,
                                         AddCommentResponse* response) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Comment"))
    return *err;
  grpc::Status status;
  auto h = resolve_host(request->host(), &status);
  if (!h)
    return status;
  const time_t entry_time =
      request->entry_time() ? request->entry_time() : time(nullptr);
  response->set_internal_id(broker_comments::publish_comment(
      h->obj().host_id(), 0, h->obj().instance_id(), Comment_EntryType_USER,
      Comment_Src_EXTERNAL, request->user(), request->comment_data(),
      request->persistent(), entry_time));
  return grpc::Status::OK;
}

/**
 * @brief Add a user comment on a service (notification_mode = broker): publish
 * a USER / EXTERNAL comment and return its internal_id.
 */
grpc::Status broker_impl::AddServiceComment(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceCommentRequest* request,
    AddCommentResponse* response) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Comment"))
    return *err;
  grpc::Status status;
  auto s = resolve_service(request->service(), &status);
  if (!s)
    return status;
  auto h = config::applier::state::instance().cache().host(s->obj().host_id());
  const time_t entry_time =
      request->entry_time() ? request->entry_time() : time(nullptr);
  response->set_internal_id(broker_comments::publish_comment(
      s->obj().host_id(), s->obj().service_id(), h ? h->obj().instance_id() : 0,
      Comment_EntryType_USER, Comment_Src_EXTERNAL, request->user(),
      request->comment_data(), request->persistent(), entry_time));
  return grpc::Status::OK;
}

/**
 * @brief Delete a comment by internal_id (notification_mode = broker). Only
 * ids minted by Broker are accepted: they identify their row platform-wide,
 * whereas an id minted by a poller would need the poller to be unambiguous.
 */
grpc::Status broker_impl::DeleteComment(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const CommentIdentifier* request,
                                        ::google::protobuf::Empty* response
                                        [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Comment"))
    return *err;
  if (request->internal_id() == 0)
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "internal_id must be set");
  if (!broker_comments::is_broker_comment_id(request->internal_id()))
    return grpc::Status(
        grpc::StatusCode::FAILED_PRECONDITION,
        fmt::format("comment {} was not created by Broker: delete it "
                    "through its poller",
                    request->internal_id()));
  broker_comments::publish_comment_deletion(request->internal_id(), 0);
  return grpc::Status::OK;
}

/**
 * @brief Delete every comment of a host (notification_mode = broker).
 */
grpc::Status broker_impl::DeleteAllHostComments(
    grpc::ServerContext* context [[maybe_unused]],
    const HostIdentifier* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Comment"))
    return *err;
  grpc::Status status;
  auto h = resolve_host(*request, &status);
  if (!h)
    return status;
  broker_comments::publish_comments_deletion(h->obj().host_id(), 0,
                                             h->obj().instance_id());
  return grpc::Status::OK;
}

/**
 * @brief Delete every comment of a service (notification_mode = broker).
 */
grpc::Status broker_impl::DeleteAllServiceComments(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceIdentifier* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Comment"))
    return *err;
  grpc::Status status;
  auto s = resolve_service(*request, &status);
  if (!s)
    return status;
  auto h = config::applier::state::instance().cache().host(s->obj().host_id());
  broker_comments::publish_comments_deletion(s->obj().host_id(),
                                             s->obj().service_id(),
                                             h ? h->obj().instance_id() : 0);
  return grpc::Status::OK;
}

/**
 * @brief Acknowledge a host problem (notification_mode = broker). Same
 * contract as the Engine RPC of the same name.
 */
grpc::Status broker_impl::AcknowledgeHostProblem(
    grpc::ServerContext* context [[maybe_unused]],
    const AcknowledgementRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(broker_acknowledgement_manager::is_loaded(),
                                    "Acknowledgement"))
    return *err;
  auto& cache = config::applier::state::instance().cache();
  auto h = cache.host(request->host_name());
  if (!h)
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        fmt::format("could not find host '{}'", request->host_name()));

  std::string err = broker_acknowledgement_manager::instance().acknowledge(
      h->obj().host_id(), 0, request->ack_author(), request->ack_data(),
      request->type() == AcknowledgementRequest_Type_STICKY, request->notify(),
      request->persistent());
  if (!err.empty())
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, err);
  return grpc::Status::OK;
}

/**
 * @brief Acknowledge a service problem (notification_mode = broker). Same
 * contract as the Engine RPC of the same name.
 */
grpc::Status broker_impl::AcknowledgeServiceProblem(
    grpc::ServerContext* context [[maybe_unused]],
    const AcknowledgementRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(broker_acknowledgement_manager::is_loaded(),
                                    "Acknowledgement"))
    return *err;
  auto& cache = config::applier::state::instance().cache();
  auto s = cache.service(request->host_name(), request->service_desc());
  if (!s)
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        fmt::format("could not find service '{}' on host '{}'",
                    request->service_desc(), request->host_name()));

  std::string err = broker_acknowledgement_manager::instance().acknowledge(
      s->obj().host_id(), s->obj().service_id(), request->ack_author(),
      request->ack_data(),
      request->type() == AcknowledgementRequest_Type_STICKY, request->notify(),
      request->persistent());
  if (!err.empty())
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, err);
  return grpc::Status::OK;
}

/**
 * @brief Remove a host acknowledgement (notification_mode = broker).
 */
grpc::Status broker_impl::RemoveHostAcknowledgement(
    grpc::ServerContext* context [[maybe_unused]],
    const HostIdentifier* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(broker_acknowledgement_manager::is_loaded(),
                                    "Acknowledgement"))
    return *err;
  grpc::Status status;
  auto h = resolve_host(*request, &status);
  if (!h)
    return status;

  std::string err =
      broker_acknowledgement_manager::instance().remove(h->obj().host_id(), 0);
  if (!err.empty())
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, err);
  return grpc::Status::OK;
}

/**
 * @brief Remove a service acknowledgement (notification_mode = broker).
 */
grpc::Status broker_impl::RemoveServiceAcknowledgement(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceIdentifier* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(broker_acknowledgement_manager::is_loaded(),
                                    "Acknowledgement"))
    return *err;
  grpc::Status status;
  auto s = resolve_service(*request, &status);
  if (!s)
    return status;

  std::string err = broker_acknowledgement_manager::instance().remove(
      s->obj().host_id(), s->obj().service_id());
  if (!err.empty())
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, err);
  return grpc::Status::OK;
}

/**
 * @brief Enable or disable the notifications of a host and, per scope, of its
 * services and/or child hosts (notification_mode = broker).
 */
grpc::Status broker_impl::SetHostNotifications(
    grpc::ServerContext* context [[maybe_unused]],
    const HostNotificationsRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  grpc::Status status;
  auto h = resolve_host(request->host(), &status);
  if (!h)
    return status;

  using cache::notification_toggles::scope;
  scope sc;
  switch (request->scope()) {
    case HostNotificationsRequest::HOST:
      sc = scope::host;
      break;
    case HostNotificationsRequest::HOST_AND_SERVICES:
      sc = scope::host_and_services;
      break;
    case HostNotificationsRequest::HOST_AND_CHILDREN:
      sc = scope::host_and_children;
      break;
    case HostNotificationsRequest::BEYOND_HOST:
      sc = scope::beyond_host;
      break;
    default:
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "unknown scope");
  }
  uint32_t count = cache::notification_toggles::set_host_notifications(
      config::applier::state::instance().cache(), h->obj().host_id(),
      request->enabled(), sc);
  _logger->info("notifications {} on host {} (scope {}): {} resource(s)",
                request->enabled() ? "enabled" : "disabled", h->obj().host_id(),
                HostNotificationsRequest::Scope_Name(request->scope()), count);
  return grpc::Status::OK;
}

/**
 * @brief Enable or disable the notifications of a service
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetServiceNotifications(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceNotificationsRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  grpc::Status status;
  auto s = resolve_service(request->service(), &status);
  if (!s)
    return status;
  cache::notification_toggles::set_service_notifications(
      config::applier::state::instance().cache(), s->obj().host_id(),
      s->obj().service_id(), request->enabled());
  _logger->info("notifications {} on service ({}, {})",
                request->enabled() ? "enabled" : "disabled", s->obj().host_id(),
                s->obj().service_id());
  return grpc::Status::OK;
}

/**
 * @brief The notification dispatcher of the Broker, nullptr when Broker does
 * not own the notification decision (notification_mode != broker).
 */
static broker_notification_dispatcher* _notification_dispatcher() {
  return static_cast<config::applier::broker_state&>(
             config::applier::state::instance())
      .notification_dispatcher();
}

/**
 * @brief Combine the custom notification flags into the library options.
 */
static com::centreon::common::notifications::notification_option
_custom_options(bool broadcast, bool forced, bool increment) {
  namespace notif = com::centreon::common::notifications;
  uint32_t o = notif::notification_option_none;
  if (broadcast)
    o |= notif::notification_option_broadcast;
  if (forced)
    o |= notif::notification_option_forced;
  if (increment)
    o |= notif::notification_option_increment;
  return static_cast<notif::notification_option>(o);
}

/**
 * @brief Set the notification number of a host (notification_mode = broker).
 */
grpc::Status broker_impl::SetHostNotificationNumber(
    grpc::ServerContext* context [[maybe_unused]],
    const HostNotificationNumberRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  auto* d = _notification_dispatcher();
  if (auto err = unavailable_unless(d != nullptr, "Notification"))
    return *err;
  grpc::Status status;
  auto h = resolve_host(request->host(), &status);
  if (!h)
    return status;
  d->post_set_notification_number(h->obj().host_id(), 0, request->number());
  return grpc::Status::OK;
}

/**
 * @brief Set the notification number of a service (notification_mode =
 * broker).
 */
grpc::Status broker_impl::SetServiceNotificationNumber(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceNotificationNumberRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  auto* d = _notification_dispatcher();
  if (auto err = unavailable_unless(d != nullptr, "Notification"))
    return *err;
  grpc::Status status;
  auto s = resolve_service(request->service(), &status);
  if (!s)
    return status;
  d->post_set_notification_number(s->obj().host_id(), s->obj().service_id(),
                                  request->number());
  return grpc::Status::OK;
}

/**
 * @brief Send a custom notification on a host (notification_mode = broker).
 */
grpc::Status broker_impl::SendCustomHostNotification(
    grpc::ServerContext* context [[maybe_unused]],
    const HostCustomNotificationRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  auto* d = _notification_dispatcher();
  if (auto err = unavailable_unless(d != nullptr, "Notification"))
    return *err;
  grpc::Status status;
  auto h = resolve_host(request->host(), &status);
  if (!h)
    return status;
  d->post_notify(h->obj().host_id(), 0,
                 com::centreon::common::notifications::reason_custom,
                 request->author(), request->comment(),
                 _custom_options(request->broadcast(), request->forced(),
                                 request->increment()));
  return grpc::Status::OK;
}

/**
 * @brief Send a custom notification on a service (notification_mode =
 * broker).
 */
grpc::Status broker_impl::SendCustomServiceNotification(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceCustomNotificationRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  auto* d = _notification_dispatcher();
  if (auto err = unavailable_unless(d != nullptr, "Notification"))
    return *err;
  grpc::Status status;
  auto s = resolve_service(request->service(), &status);
  if (!s)
    return status;
  d->post_notify(s->obj().host_id(), s->obj().service_id(),
                 com::centreon::common::notifications::reason_custom,
                 request->author(), request->comment(),
                 _custom_options(request->broadcast(), request->forced(),
                                 request->increment()));
  return grpc::Status::OK;
}

/**
 * @brief Check that a notification timeperiod named in a request is known to
 * the Broker cache.
 *
 * @param cache  The Broker cache.
 * @param name   The timeperiod name.
 * @param status Set when false is returned.
 *
 * @return True when the timeperiod exists.
 */
static bool _check_timeperiod(const cache::broker_cache& bc,
                              const std::string& name,
                              grpc::Status* status) {
  if (name.empty()) {
    *status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                           "timeperiod must be set");
    return false;
  }
  if (!bc.has_timeperiod(name)) {
    *status = grpc::Status(grpc::StatusCode::NOT_FOUND,
                           fmt::format("unknown timeperiod '{}'", name));
    return false;
  }
  return true;
}

/**
 * @brief Change the notification timeperiod of a host
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetHostNotificationPeriod(
    grpc::ServerContext* context [[maybe_unused]],
    const HostNotificationPeriodRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  grpc::Status status;
  auto& bc = config::applier::state::instance().cache();
  auto h = resolve_host(request->host(), &status);
  if (!h)
    return status;
  if (!_check_timeperiod(bc, request->timeperiod(), &status))
    return status;
  bc.set_notification_period(h->obj().host_id(), 0, request->timeperiod());
  _logger->info("notification period of host {} set to '{}'",
                h->obj().host_id(), request->timeperiod());
  return grpc::Status::OK;
}

/**
 * @brief Change the notification timeperiod of a service
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetServiceNotificationPeriod(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceNotificationPeriodRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  grpc::Status status;
  auto& bc = config::applier::state::instance().cache();
  auto s = resolve_service(request->service(), &status);
  if (!s)
    return status;
  if (!_check_timeperiod(bc, request->timeperiod(), &status))
    return status;
  bc.set_notification_period(s->obj().host_id(), s->obj().service_id(),
                             request->timeperiod());
  _logger->info("notification period of service ({}, {}) set to '{}'",
                s->obj().host_id(), s->obj().service_id(),
                request->timeperiod());
  return grpc::Status::OK;
}

/**
 * @brief Shared body of SetContact{Host,Service}Notifications.
 */
grpc::Status broker_impl::_set_contact_notifications(
    const ContactNotificationsRequest& request,
    cache::notification_toggles::notifier n) const {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  auto& bc = config::applier::state::instance().cache();
  const std::string& name = request.contact().name();
  if (name.empty())
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "contact name must be set");
  if (!cache::notification_toggles::set_contact_notifications(
          bc, name, n, request.enabled()))
    return grpc::Status(grpc::StatusCode::NOT_FOUND,
                        fmt::format("unknown contact '{}'", name));
  _logger->info(
      "{} notifications {} on contact '{}'",
      n == cache::notification_toggles::notifier::host ? "host" : "service",
      request.enabled() ? "enabled" : "disabled", name);
  return grpc::Status::OK;
}

/**
 * @brief Enable or disable the host notifications of a contact
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetContactHostNotifications(
    grpc::ServerContext* context [[maybe_unused]],
    const ContactNotificationsRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  return _set_contact_notifications(
      *request, cache::notification_toggles::notifier::host);
}

/**
 * @brief Enable or disable the service notifications of a contact
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetContactServiceNotifications(
    grpc::ServerContext* context [[maybe_unused]],
    const ContactNotificationsRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  return _set_contact_notifications(
      *request, cache::notification_toggles::notifier::service);
}

/**
 * @brief Shared body of SetContactgroup{Host,Service}Notifications.
 */
grpc::Status broker_impl::_set_contactgroup_notifications(
    const ContactgroupNotificationsRequest& request,
    cache::notification_toggles::notifier n) const {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  auto& bc = config::applier::state::instance().cache();
  const std::string& name = request.contactgroup().name();
  if (name.empty())
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "contactgroup name must be set");
  auto count = cache::notification_toggles::set_contactgroup_notifications(
      bc, name, n, request.enabled());
  if (!count)
    return grpc::Status(grpc::StatusCode::NOT_FOUND,
                        fmt::format("unknown contactgroup '{}'", name));
  _logger->info(
      "{} notifications {} on contactgroup '{}': {} contact(s)",
      n == cache::notification_toggles::notifier::host ? "host" : "service",
      request.enabled() ? "enabled" : "disabled", name, *count);
  return grpc::Status::OK;
}

/**
 * @brief Enable or disable the host notifications of the contacts of a
 * contactgroup (notification_mode = broker).
 */
grpc::Status broker_impl::SetContactgroupHostNotifications(
    grpc::ServerContext* context [[maybe_unused]],
    const ContactgroupNotificationsRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  return _set_contactgroup_notifications(
      *request, cache::notification_toggles::notifier::host);
}

/**
 * @brief Enable or disable the service notifications of the contacts of a
 * contactgroup (notification_mode = broker).
 */
grpc::Status broker_impl::SetContactgroupServiceNotifications(
    grpc::ServerContext* context [[maybe_unused]],
    const ContactgroupNotificationsRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  return _set_contactgroup_notifications(
      *request, cache::notification_toggles::notifier::service);
}

/**
 * @brief Shared body of SetContact{Host,Service}NotificationPeriod.
 */
grpc::Status broker_impl::_set_contact_notification_period(
    const ContactNotificationPeriodRequest& request,
    cache::notification_toggles::notifier n) const {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  grpc::Status status;
  auto& bc = config::applier::state::instance().cache();
  const std::string& name = request.contact().name();
  if (name.empty())
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "contact name must be set");
  if (!_check_timeperiod(bc, request.timeperiod(), &status))
    return status;
  if (!cache::notification_toggles::set_contact_notification_period(
          bc, name, n, request.timeperiod()))
    return grpc::Status(grpc::StatusCode::NOT_FOUND,
                        fmt::format("unknown contact '{}'", name));
  _logger->info(
      "{} notification period of contact '{}' set to '{}'",
      n == cache::notification_toggles::notifier::host ? "host" : "service",
      name, request.timeperiod());
  return grpc::Status::OK;
}

/**
 * @brief Change the host notification timeperiod of a contact
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetContactHostNotificationPeriod(
    grpc::ServerContext* context [[maybe_unused]],
    const ContactNotificationPeriodRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  return _set_contact_notification_period(
      *request, cache::notification_toggles::notifier::host);
}

/**
 * @brief Change the service notification timeperiod of a contact
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetContactServiceNotificationPeriod(
    grpc::ServerContext* context [[maybe_unused]],
    const ContactNotificationPeriodRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  return _set_contact_notification_period(
      *request, cache::notification_toggles::notifier::service);
}

/**
 * @brief Enable or disable the notifications of a whole poller
 * (notification_mode = broker).
 */
grpc::Status broker_impl::SetPollerNotifications(
    grpc::ServerContext* context [[maybe_unused]],
    const PollerNotificationsRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  if (auto err = unavailable_unless(com::centreon::common::notifications::
                                        notification_manager::is_loaded(),
                                    "Notification"))
    return *err;
  auto& bc = config::applier::state::instance().cache();
  grpc::Status status;
  uint64_t poller_id = resolve_poller(request->poller(), &status);
  if (!status.ok())
    return status;
  if (!cache::notification_toggles::set_poller_notifications(
          bc, poller_id, request->enabled()))
    return grpc::Status(grpc::StatusCode::NOT_FOUND,
                        fmt::format("unknown poller {}", poller_id));
  _logger->info("notifications {} on poller {}",
                request->enabled() ? "enabled" : "disabled", poller_id);

  /* Broker owns the decision, but the poller still exports its own
   * enable_notifications flag (instances.notifications): make it follow the
   * switch by handing it the legacy command, so the DB and the poller agree
   * with Broker. Best effort: a disconnected poller gets the switch through
   * its configuration at reconnection anyway. */
  auto& state = static_cast<config::applier::broker_state&>(
      config::applier::state::instance());
  if (state.is_poller_connected(poller_id))
    state.push_pending_for_poller(
        poller_id, make_external_command(
                       poller_id, fmt::format("[{}] {}", time(nullptr),
                                              request->enabled()
                                                  ? "ENABLE_NOTIFICATIONS"
                                                  : "DISABLE_NOTIFICATIONS")));
  return grpc::Status::OK;
}

/**
 * @brief Route a legacy Engine external command to the poller(s) it applies
 * to. Broker parses just enough of the line to find the command name and, for
 * a host or service command, the host and service; the whole line, prefixed
 * with the entry timestamp, is then handed to the poller through the downward
 * BBDO channel (pb_external_command) and executed there by the same parser as
 * the command pipe. A host or service command carries the ids Broker
 * resolved, so the poller rewrites the names from them before parsing.
 *
 * Routing by target (common/external_commands):
 *  - host, service: the poller supervising the host;
 *  - contact, contactgroup: every connected poller (each has its copy);
 *  - global, process: the poller named in the request (required);
 *  - hostgroup, servicegroup (PHP no longer emits them), downtime and comment
 *    designated by id: UNIMPLEMENTED.
 * In notification_mode=broker the commands Broker owns (acknowledgements,
 * downtimes, comments, notification switches) are refused with
 * FAILED_PRECONDITION and the name of the gRPC method to use instead: routed
 * to the poller they would be silently ignored by Broker.
 *
 * @param request The legacy command line, with or without "[timestamp] ", and
 * the poller for a global or process command.
 * @param response Unused.
 *
 * @return OK once the command is queued for the poller(s), the error otherwise.
 */
grpc::Status broker_impl::ExecuteExternalCommand(
    grpc::ServerContext* context [[maybe_unused]],
    const ExternalCommandRequest* request,
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  namespace ec = com::centreon::common::external_commands;
  std::string_view line = absl::StripAsciiWhitespace(request->command());

  /* Optional "[timestamp] " prefix, as on the command pipe. */
  time_t entry_time = time(nullptr);
  if (absl::ConsumePrefix(&line, "[")) {
    size_t close = line.find(']');
    uint64_t ts = 0;
    if (close == std::string_view::npos ||
        !absl::SimpleAtoi(absl::StripAsciiWhitespace(line.substr(0, close)),
                          &ts))
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                          "malformed timestamp prefix, expected '[epoch] '");
    entry_time = static_cast<time_t>(ts);
    line.remove_prefix(close + 1);
    line = absl::StripLeadingAsciiWhitespace(line);
  }

  std::vector<std::string_view> fields = absl::StrSplit(line, ';');
  std::string_view name = fields[0];
  if (name.empty())
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "empty external command");
  auto info = ec::lookup(name);
  if (!info)
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        fmt::format("unknown external command '{}'", name));

  auto& state = static_cast<config::applier::broker_state&>(
      config::applier::state::instance());
  if (state.notifications_on_broker() && !info->broker_rpc.empty())
    return grpc::Status(
        grpc::StatusCode::FAILED_PRECONDITION,
        fmt::format("{} is handled by Broker in notification_mode=broker: use "
                    "the {} gRPC method",
                    name, info->broker_rpc));

  const std::string full_line = fmt::format("[{}] {}", entry_time, line);
  switch (info->kind) {
    case ec::target::host:
    case ec::target::service: {
      if (fields.size() < 2 || fields[1].empty())
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            fmt::format("{} expects a host name as first argument", name));
      std::string host_name(fields[1]);
      auto h = state.cache().host(host_name);
      if (!h)
        return grpc::Status(grpc::StatusCode::NOT_FOUND,
                            fmt::format("unknown host '{}'", host_name));
      uint64_t poller_id = h->obj().instance_id();
      uint64_t host_id = h->obj().host_id();
      uint64_t service_id = 0;
      if (info->kind == ec::target::service) {
        if (fields.size() < 3 || fields[2].empty())
          return grpc::Status(
              grpc::StatusCode::INVALID_ARGUMENT,
              fmt::format("{} expects a service description as second argument",
                          name));
        std::string description(fields[2]);
        auto s = state.cache().service(host_name, description);
        if (!s)
          return grpc::Status(grpc::StatusCode::NOT_FOUND,
                              fmt::format("unknown service ('{}', '{}')",
                                          host_name, description));
        service_id = s->obj().service_id();
      }
      if (!state.is_poller_connected(poller_id))
        return grpc::Status(
            grpc::StatusCode::UNAVAILABLE,
            fmt::format("poller {} supervising '{}' is not connected",
                        poller_id, host_name));
      state.push_pending_for_poller(
          poller_id,
          make_external_command(poller_id, full_line, host_id, service_id));
      _logger->info("external command {} routed to poller {}", name, poller_id);
      return grpc::Status::OK;
    }
    case ec::target::contact:
    case ec::target::contactgroup: {
      if (fields.size() < 2 || fields[1].empty())
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            fmt::format("{} expects a {} name as first argument", name,
                        ec::to_string(info->kind)));
      /* Every poller holds its own copy of the contacts: broadcast. */
      auto pollers = state.connected_pollers();
      if (pollers.empty())
        return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                            "no poller connected");
      for (const auto& p : pollers)
        state.push_pending_for_poller(
            p.poller_id, make_external_command(p.poller_id, full_line));
      _logger->info("external command {} broadcast to {} poller(s)", name,
                    pollers.size());
      return grpc::Status::OK;
    }
    case ec::target::global:
    case ec::target::process: {
      if (!request->has_poller())
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            fmt::format("{} is a {} command: the request must name the poller",
                        name, ec::to_string(info->kind)));
      grpc::Status status;
      uint64_t poller_id = resolve_poller(request->poller(), &status);
      if (!status.ok())
        return status;
      if (!state.is_poller_connected(poller_id))
        return grpc::Status(
            grpc::StatusCode::UNAVAILABLE,
            fmt::format("poller {} is not connected", poller_id));
      state.push_pending_for_poller(
          poller_id, make_external_command(poller_id, full_line));
      _logger->info("external command {} routed to poller {}", name, poller_id);
      return grpc::Status::OK;
    }
    case ec::target::hostgroup:
    case ec::target::servicegroup:
    case ec::target::downtime:
    case ec::target::comment:
      break;
  }
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                      fmt::format("{} targets a {}: not routed by Broker", name,
                                  ec::to_string(info->kind)));
}

/**
 * @brief Validate an Engine poller configuration directory without applying it.
 *
 * Reuses the same offline pipeline as the centralized-config ingestion
 * (build_test_file + parser::parse + state_helper::expand), but routes all the
 * validation logging to a dedicated in-memory sink so the problems can be
 * returned to the caller. Broker's shared loggers are never touched.
 *
 * @param request The directory holding centengine.cfg and the other .cfg files.
 * @param response The diagnostics collected and whether the config is usable.
 *
 * @return grpc::Status::OK (validation outcome is carried in the response).
 */
grpc::Status broker_impl::CheckPollerConfig(
    grpc::ServerContext* context [[maybe_unused]],
    const CheckPollerConfigRequest* request,
    CheckPollerConfigResponse* response) {
  namespace conf = com::centreon::engine::configuration;

  // Dedicated single-threaded logger: its records are captured here instead of
  // going to broker's shared CONFIG logger (no locking, no shared state).
  auto sink = std::make_shared<capturing_sink>();
  auto logger = std::make_shared<spdlog::logger>("check-poller-config", sink);
  logger->set_level(spdlog::level::warn);  // only warnings and errors matter

  // build_test_file rewrites the cfg_file=/resource_file= paths to resolve
  // inside the directory, so the derived file must live in it.
  std::filesystem::path dir(request->directory());
  std::filesystem::path cfg = dir / "centengine.cfg";
  std::filesystem::path test = dir / "centengine.test";

  std::error_code ec;
  conf::parser::build_test_file(test, cfg, ec);
  if (ec) {
    auto* d = response->add_diagnostics();
    d->set_severity(ConfigDiagnostic_Severity_ERROR);
    d->set_message(fmt::format("cannot read the configuration in '{}': {}",
                               dir.string(), ec.message()));
    response->set_ok(false);
    return grpc::Status::OK;
  }

  /* The same cross-poller index the ingestion builds, so that both paths judge
   * a configuration identically: an object defined on another poller must read
   * as living elsewhere here too, not as undefined.
   *
   * The Broker gRPC service only runs on instances with the Broker role, so the
   * cast is safe -- same reasoning as GetPeers above. On an instance holding no
   * poller configuration (a relay), the index comes back empty and the
   * validation is simply the local one.
   */
  config::applier::broker_state* st =
      static_cast<config::applier::broker_state*>(
          &config::applier::state::instance());
  auto foreign = st->load_foreign_objects();

  /* Which poller is being validated has to be excluded from that index, or an
   * object dropped from the directory under validation would still be found in
   * the configuration stored for that same poller and read as "defined
   * elsewhere" -- letting a dangling reference through.
   *
   * A poller configuration lives in a directory named after its id, which is
   * where the ingestion takes it from as well: nothing carries it inside the
   * .cfg files. `lexically_normal` so that a trailing slash does not hide the
   * name. */
  uint32_t validated_poller = 0;
  if (absl::SimpleAtoi(dir.lexically_normal().filename().string(),
                       &validated_poller))
    foreign.set_to_exclude(validated_poller);
  else if (!foreign.empty()) {
    /* Nothing says which poller this is, so nothing can be excluded. Said out
     * loud rather than silently accepted: a reference to an object this very
     * directory has just dropped would be taken for one living elsewhere. */
    auto* d = response->add_diagnostics();
    d->set_severity(ConfigDiagnostic_Severity_WARNING);
    d->set_message(fmt::format(
        "'{}' is not named after a poller id, so the objects stored for the "
        "poller it belongs to cannot be told apart from those of the others: a "
        "reference to an object removed from this very configuration may go "
        "unreported",
        dir.string()));
  }

  conf::State state;
  conf::state_helper state_hlp(&state);
  conf::error_cnt err;
  conf::parser p{logger};
  std::string thrown;
  try {
    p.parse(test.string(), &state, err);
    state_hlp.expand(err, logger);
    state_hlp.resolve(err, logger, foreign);
  } catch (const std::exception& e) {
    thrown = e.what();
  }

  std::filesystem::remove(test, ec);
  if (ec)
    SPDLOG_LOGGER_ERROR(
        _logger, "CheckPollerConfig: cannot remove the derived file '{}': {}",
        test.string(), ec.message());

  // Logged warnings/errors first (chronological), then the fatal exception.
  for (const auto& r : sink->records) {
    auto* d = response->add_diagnostics();
    d->set_severity(r.level >= spdlog::level::err
                        ? ConfigDiagnostic_Severity_ERROR
                        : ConfigDiagnostic_Severity_WARNING);
    d->set_message(r.message);
  }
  if (!thrown.empty()) {
    auto* d = response->add_diagnostics();
    d->set_severity(ConfigDiagnostic_Severity_ERROR);
    d->set_message(thrown);
  }

  response->set_ok(err.config_errors == 0 && thrown.empty());
  return grpc::Status::OK;
}
