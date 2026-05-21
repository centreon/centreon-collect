/**
 * Copyright 2024 Centreon
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

#include <google/protobuf/util/message_differencer.h>

#include "centreon_agent/agent_impl.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/base64.hh"

#include "otl_fmt.hh"

#include "com/centreon/engine/command_manager.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

/**
 * @brief when BiReactor::OnDone is called by grpc layers, we should delete
 * this. But this object is even used by others.
 * So it's stored in these container and just removed from this container when
 * OnDone is called
 * This container is also used to push configuration changes to agent
 *
 */
agent_impl_base::instance_container* agent_impl_base::_configured_instance =
    new agent_impl_base::instance_container;

absl::btree_set<std::shared_ptr<agent_impl_base>>*
    agent_impl_base::_no_configured_instance =
        new absl::btree_set<std::shared_ptr<agent_impl_base>>;

absl::Mutex* agent_impl_base::_instances_m = new absl::Mutex;

/**
 * @brief stores connections along their host and service ids
 *
 * @param conf
 */
void agent_impl_base::_on_new_conf(const agent::AgentConfiguration& conf) {
  absl::MutexLock l(_instances_m);
  auto me = shared_from_this();
  _configured_instance->get<1>().erase(me);
  if (conf.services().empty()) {
    _no_configured_instance->insert(me);
  } else {
    _no_configured_instance->erase(me);

    for (const agent::Service& serv : conf.services()) {
      _configured_instance->emplace(serv.host_id(), serv.service_id(), me);
    }
  }
}

/**
 * @brief when grpc Layers call OnDone, we can remove connection from containers
 *
 */
void agent_impl_base::_on_done() {
  auto me = shared_from_this();
  absl::MutexLock l(_instances_m);
  _configured_instance->get<1>().erase(me);
  _no_configured_instance->erase(me);
}

/**
 * @brief static method used to push new configuration to all agents
 *
 */
void agent_impl_base::all_agent_calc_and_send_config_if_needed(
    const agent_config::pointer& new_conf) {
  _apply_to_all([&new_conf](const agent_impl_base::pointer& conn) {
    conn->calc_and_send_config_if_needed(new_conf);
  });
}

/**
 * @brief static version of _force_check, it search connection that handle
 * host_id and serv_id and then call _force_check
 *
 * @param host_id
 * @param serv_id
 */
void agent_impl_base::force_check(uint64_t host_id, uint64_t serv_id) {
  absl::MutexLock l(_instances_m);
  auto& host_serv_index = _configured_instance->get<0>();
  auto search = host_serv_index.find(std::make_pair(host_id, serv_id));
  if (search == host_serv_index.end()) {
    throw exceptions::msg_fmt(
        "No agent that checks service {} of host {} connected", serv_id,
        host_id);
  }
  search->connection->_force_check(host_id, serv_id);
}

/**
 * @brief Construct a new agent impl<bireactor class>::agent impl object
 *
 * @tparam bireactor_class
 * @param io_context
 * @param class_name
 * @param handler handler that will process received metrics
 * @param logger
 */
template <class bireactor_class>
agent_impl<bireactor_class>::agent_impl(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::string_view class_name,
    const agent_config::pointer& conf,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    bool reversed,
    bool is_crypted,
    const agent_stat::pointer& stats)
    : _io_context(io_context),
      _class_name(class_name),
      _reversed(reversed),
      _is_crypted(is_crypted),
      _exp_time(std::chrono::system_clock::time_point::min()),
      _conf(conf),
      _metric_handler(handler),
      _write_pending(false),
      _logger(logger),
      _alive(true),
      _stats(stats) {
  SPDLOG_LOGGER_DEBUG(logger, "create {} this={:p}", _class_name,
                      static_cast<const void*>(this));
}

template <class bireactor_class>
agent_impl<bireactor_class>::agent_impl(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const std::string_view class_name,
    const agent_config::pointer& conf,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    bool reversed,
    bool is_crypted,
    const agent_stat::pointer& stats,
    const std::chrono::system_clock::time_point& exp_time)
    : _io_context(io_context),
      _class_name(class_name),
      _reversed(reversed),
      _is_crypted(is_crypted),
      _exp_time(exp_time),
      _conf(conf),
      _agent_can_receive_encrypted_credentials(false),
      _metric_handler(handler),
      _write_pending(false),
      _logger(logger),
      _alive(true),
      _stats(stats) {
  SPDLOG_LOGGER_DEBUG(logger, "create {} this={:p}", _class_name,
                      static_cast<const void*>(this));
}

/**
 * @brief Destroy the agent impl<bireactor class>::agent impl object
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
agent_impl<bireactor_class>::~agent_impl() {
  if (_agent_info && _agent_info->has_init()) {
    _stats->remove_agent(_agent_info->init(), _reversed, this);
  }
  SPDLOG_LOGGER_DEBUG(_logger, "delete {} this={:p}", _class_name,
                      static_cast<const void*>(this));
}

/**
 * @brief just call _calc_and_send_config_if_needed in main engine thread
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::calc_and_send_config_if_needed(
    const agent_config::pointer& new_conf) {
  {
    absl::MutexLock l(&_protect);
    _conf = new_conf;
  }
  auto to_call = std::packaged_task<int(void)>(
      [me = shared_from_this()]() mutable -> int32_t {
        // then we are in the main thread
        // services, hosts and commands are stable
        me->_calc_and_send_config_if_needed();
        return 0;
      });
  command_manager::instance().enqueue(std::move(to_call));
}

static bool add_command_to_agent_conf(
    const std::string& cmd_name,
    const std::string& cmd_line,
    const std::string& service,
    uint64_t host_id,
    uint64_t service_id,
    uint32_t check_interval,
    uint32_t retry_interval,
    uint32_t max_attempts,
    const std::string& check_period_name,
    com::centreon::agent::AgentConfiguration* cnf,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& peer,
    bool encrypt_credentials) {
  std::string plugins_cmdline = boost::trim_copy(cmd_line);

  if (plugins_cmdline.empty()) {
    SPDLOG_LOGGER_ERROR(
        logger,
        "Failed to add command for agent '{}', service '{}': plugins command "
        "line is empty (original command line: '{}')",
        peer, service, cmd_line);
    return false;
  }

  SPDLOG_LOGGER_TRACE(
      logger,
      "Add command to agent: {}, serv: {}, cmd {} plugins command line {}",
      peer, service, cmd_name, cmd_line);

  com::centreon::agent::Service* serv = cnf->add_services();
  serv->set_service_description(service);
  serv->set_command_name(cmd_name);
  serv->set_host_id(host_id);
  serv->set_service_id(service_id);
  if (encrypt_credentials && credentials_decrypt) {
    serv->set_command_line("encrypt::" +
                           credentials_decrypt->encrypt(plugins_cmdline));
  } else {
    serv->set_command_line(plugins_cmdline);
  }
  serv->set_check_interval(check_interval * pb_config.interval_length());
  serv->set_retry_interval(retry_interval * pb_config.interval_length());
  serv->set_max_attempts(max_attempts);
  if (!check_period_name.empty()) {
    serv->set_check_period_name(check_period_name);
  }

  return true;
}

// copy the matching configuration::Timeperiod protos into cnf.
static void _serialize_timeperiods(
    com::centreon::agent::AgentConfiguration* cnf) {
  absl::flat_hash_set<std::string> seen;

  for (const auto& svc : cnf->services()) {
    if (!svc.check_period_name().empty())
      seen.insert(svc.check_period_name());
  }

  for (const auto& tp : pb_config.timeperiods()) {
    if (!tp.timeperiod_name().empty() &&
        seen.find(tp.timeperiod_name()) != seen.end()) {
      auto timeperiod = cnf->add_timeperiods();
      timeperiod->CopyFrom(tp);
    }
  }
}

/**
 * @brief this function must be called in the engine main thread
 * It calculates agent configuration, if different to the older, it sends it to
 * agent
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::_calc_and_send_config_if_needed() {
  std::shared_ptr<agent::MessageToAgent> new_conf =
      std::make_shared<agent::MessageToAgent>();
  {
    agent::AgentConfiguration* cnf = new_conf->mutable_config();
    cnf->set_check_timeout(_conf->get_check_timeout());
    cnf->set_export_period(_conf->get_export_period());
    cnf->set_max_concurrent_checks(_conf->get_max_concurrent_checks());
    cnf->set_use_exemplar(true);
    bool crypt_credentials = false;
    if (!_is_crypted) {
      SPDLOG_LOGGER_INFO(_logger,
                         "As connection is not encrypted, Engine will send no "
                         "encrypted credentials to agent {}",
                         *new_conf);
    } else if (!_agent_can_receive_encrypted_credentials) {
      SPDLOG_LOGGER_INFO(
          _logger,
          "Agent is not credentials encrypted ready, Engine will send no "
          "encrypted credentials to agent {}",
          *new_conf);
    } else if (credentials_decrypt) {
      cnf->set_key(
          common::crypto::base64_encode(credentials_decrypt->first_key()));
      cnf->set_salt(
          common::crypto::base64_encode(credentials_decrypt->second_key()));
      SPDLOG_LOGGER_INFO(_logger,
                         "Engine will send encrypted credentials to agent {}",
                         *new_conf);
      crypt_credentials = true;
    } else {
      SPDLOG_LOGGER_INFO(
          _logger, "Engine will send no encrypted credentials to agent {}",
          *new_conf);
    }

    absl::MutexLock l(&_protect);
    if (!_alive) {
      return;
    }
    if (_agent_info) {
      const std::string& peer = get_peer();
      e_get_otel_commands_ret command_added = get_otel_commands(
          _agent_info->init().host(),
          [cnf, &peer, crypt_credentials](
              const std::string& cmd_name, const std::string& cmd_line,
              const std::string& service, uint64_t host_id, uint64_t service_id,
              uint32_t check_interval, uint32_t retry_interval,
              uint32_t max_attempts, const std::string& check_period_name,
              const std::shared_ptr<spdlog::logger>& logger) {
            return add_command_to_agent_conf(
                cmd_name, cmd_line, service, host_id, service_id,
                check_interval, retry_interval, max_attempts, check_period_name,
                cnf, logger, peer, crypt_credentials);
          },
          _whitelist_cache, _logger);
      if (command_added == e_get_otel_commands_ret::no_cma_service) {
        SPDLOG_LOGGER_ERROR(_logger, "No command found for agent {} : {}",
                            get_peer(), _agent_info->init().ShortDebugString());
      } else if (command_added == e_get_otel_commands_ret::unknown_host) {
        SPDLOG_LOGGER_ERROR(_logger, "unknown host for agent {} : {}",
                            get_peer(), _agent_info->init().ShortDebugString());
        // notify broker of an unknown cma host
        com::centreon::broker::UnknownHost to_send;
        const auto& agent_info = _agent_info->init();
        to_send.set_host_name(agent_info.host());
        to_send.set_host_template(agent_info.host_template());
        to_send.set_os(agent_info.os());
        to_send.set_os_version(agent_info.os_version());
        for (const auto& ip : agent_info.ips()) {
          to_send.add_ips(ip);
        }
        to_send.set_incoming_ip(get_peer());
        broker_agent_unknown_host(to_send);
      }
    }
    _serialize_timeperiods(cnf);

    if (!_last_sent_config ||
        !::google::protobuf::util::MessageDifferencer::Equals(
            *cnf, _last_sent_config->config())) {
      _on_new_conf(new_conf->config());
      _last_sent_config = new_conf;
    } else {
      new_conf.reset();
      SPDLOG_LOGGER_DEBUG(_logger, "No need to update conf for agent: {}",
                          get_peer());
    }
  }
  if (new_conf) {
    SPDLOG_LOGGER_DEBUG(_logger, "Send conf to agent: {}", get_peer());
    _write(new_conf);
  }
}

/**
 * @brief manages incoming request (init or otel data)
 *
 * @tparam bireactor_class
 * @param request
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::on_request(
    const std::shared_ptr<agent::MessageFromAgent>& request) {
  agent_config::pointer agent_conf;
  if (request->has_init()) {
    {
      absl::MutexLock l(&_protect);
      _agent_info = request;
      agent_conf = _conf;
      _last_sent_config.reset();
      _agent_can_receive_encrypted_credentials =
          _agent_info->init().encryption_ready();
    }
    _stats->add_agent(_agent_info->init(), _reversed, this);
    SPDLOG_LOGGER_DEBUG(_logger, "init from {}", get_peer());
    calc_and_send_config_if_needed(agent_conf);
  }
  if (request->has_otel_request()) {
    metric_request_ptr received(request->unsafe_arena_release_otel_request());
    _metric_handler(received);
  }
}

/**
 * @brief send request to agent
 *
 * @tparam bireactor_class
 * @param request
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::_write(
    const std::shared_ptr<agent::MessageToAgent>& request) {
  {
    absl::MutexLock l(&_protect);
    if (!_alive) {
      return;
    }
    _write_queue.push_back(request);
  }
  start_write();
}

/**
 * @brief all grpc streams are stored in an static container
 *
 * @tparam bireactor_class
 * @param strm
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::register_stream(
    const std::shared_ptr<agent_impl>& strm) {
  strm->_on_new_conf(agent::AgentConfiguration());
}

/**
 * @brief start an asynchronous read
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::start_read() {
  absl::MutexLock l(&_protect);
  if (!_alive) {
    return;
  }
  std::shared_ptr<agent::MessageFromAgent> to_read;
  if (_read_current) {
    return;
  }
  to_read = _read_current = std::make_shared<agent::MessageFromAgent>();
  bireactor_class::StartRead(to_read.get());
}

/**
 * @brief we have receive a request or an eof
 *
 * @tparam bireactor_class
 * @param ok
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::OnReadDone(bool ok) {
  if (ok) {
    if (_exp_time != std::chrono::system_clock::time_point::min() &&
        _exp_time != std::chrono::system_clock::time_point::max() &&
        _exp_time <= std::chrono::system_clock::now()) {
      SPDLOG_LOGGER_ERROR(_logger, "{:p} {} token expired",
                          static_cast<void*>(this), _class_name);
      on_error();
      this->shutdown();
      return;
    }
    std::shared_ptr<agent::MessageFromAgent> readden;
    {
      absl::MutexLock l(&_protect);
      SPDLOG_LOGGER_TRACE(_logger, "{:p} {} receive from {}: {}",
                          static_cast<const void*>(this), _class_name,
                          get_peer(), *_read_current);
      readden = _read_current;
      _read_current.reset();
    }
    start_read();
    on_request(readden);
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} {} fail read from {}",
                        static_cast<void*>(this), _class_name, get_peer());
    on_error();
    this->shutdown();
  }
}

/**
 * @brief starts an asynchronous write
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::start_write() {
  std::shared_ptr<agent::MessageToAgent> to_send;
  {
    absl::MutexLock l(&_protect);
    if (!_alive || _write_pending || _write_queue.empty()) {
      return;
    }
    to_send = _write_queue.front();
    _write_pending = true;
  }
  SPDLOG_LOGGER_TRACE(_logger, "{:p} {} send to {}: {}",
                      static_cast<void*>(this), _class_name, get_peer(),
                      *to_send);
  bireactor_class::StartWrite(to_send.get());
}

/**
 * @brief write handler
 *
 * @tparam bireactor_class
 * @param ok
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::OnWriteDone(bool ok) {
  if (ok) {
    {
      absl::MutexLock l(&_protect);
      _write_pending = false;
      SPDLOG_LOGGER_TRACE(_logger, "{:p} {} {} sent",
                          static_cast<const void*>(this), _class_name,
                          **_write_queue.begin());
      _write_queue.pop_front();
    }
    start_write();
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "{:p} {} fail write to stream",
                        static_cast<void*>(this), _class_name);
    on_error();
    this->shutdown();
  }
}

/**
 * @brief called when server agent connection is closed
 * When grpc layers call this handler, oject must be deleted
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::OnDone() {
  /**grpc has a bug, sometimes if we delete this class in this handler as it is
   * described in examples, it also deletes used channel and does a pthread_join
   * of the current thread witch go to a EDEADLOCK error and call grpc::Crash.
   * So we uses asio thread to do the job
   */
  asio::post(*_io_context, [me = shared_from_this(), logger = _logger]() {
    SPDLOG_LOGGER_DEBUG(logger, "{:p} server::OnDone()",
                        static_cast<void*>(me.get()));
    me->_on_done();
  });
}

/**
 * @brief called when client agent connection is closed
 * When grpc layers call this handler, oject must be deleted
 *
 * @tparam bireactor_class
 * @param status status passed to Finish agent side method
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::OnDone(const ::grpc::Status& status) {
  /**grpc has a bug, sometimes if we delete this class in this handler as it is
   * described in examples, it also deletes used channel and does a
   * pthread_join of the current thread witch go to a EDEADLOCK error and call
   * grpc::Crash. So we uses asio thread to do the job
   */
  asio::post(
      *_io_context, [me = shared_from_this(), status, logger = _logger]() {
        if (status.ok()) {
          SPDLOG_LOGGER_DEBUG(logger, "{:p} client::OnDone({}) {}",
                              static_cast<void*>(me.get()),
                              status.error_message(), status.error_details());
        } else {
          SPDLOG_LOGGER_ERROR(logger, "{:p} client::OnDone({}) {}",
                              static_cast<void*>(me.get()),
                              status.error_message(), status.error_details());
        }
        me->_on_done();
      });
}

/**
 * @brief just log, must be inherited
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::shutdown() {
  SPDLOG_LOGGER_DEBUG(_logger, "{:p} {}::shutdown", static_cast<void*>(this),
                      _class_name);
}

/**
 * @brief static method used to shutdown all connections
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::shutdown_all() {
  _apply_to_all([](const agent_impl_base::pointer& conn) { conn->shutdown(); });
}

/**
 * @brief send force check message to the agent
 *
 * @tparam bireactor_class
 * @param host_id
 * @param serv_id
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::_force_check(uint64_t host_id,
                                               uint64_t serv_id) {
  std::shared_ptr<agent::MessageToAgent> to_agent =
      std::make_shared<agent::MessageToAgent>();

  auto force = to_agent->mutable_force_check();
  force->set_host_id(host_id);
  force->set_serv_id(serv_id);
  _write(to_agent);
}

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

template class agent_impl<
    ::grpc::ClientBidiReactor<agent::MessageToAgent, agent::MessageFromAgent>>;

template class agent_impl<
    ::grpc::ServerBidiReactor<agent::MessageFromAgent, agent::MessageToAgent>>;

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent
