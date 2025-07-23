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
#include "com/centreon/engine/globals.hh"
#include "common/crypto/base64.hh"

#include "otl_fmt.hh"

#include "com/centreon/engine/command_manager.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

/**
 * @brief when BiReactor::OnDone is called by grpc layers, we should delete
 * this. But this object is even used by others.
 * So it's stored in this container and just removed from this container when
 * OnDone is called
 * This container is also used to push configuration changes to agent
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
std::set<std::shared_ptr<agent_impl<bireactor_class>>>*
    agent_impl<bireactor_class>::_instances =
        new std::set<std::shared_ptr<agent_impl<bireactor_class>>>;

template <class bireactor_class>
absl::Mutex agent_impl<bireactor_class>::_instances_m;

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
      [me = std::enable_shared_from_this<agent_impl<bireactor_class>>::
           shared_from_this()]() mutable -> int32_t {
        // then we are in the main thread
        // services, hosts and commands are stable
        me->_calc_and_send_config_if_needed();
        return 0;
      });
  command_manager::instance().enqueue(std::move(to_call));
}

/**
 * @brief static method used to push new configuration to all agents
 *
 * @tparam bireactor_class
 */
template <class bireactor_class>
void agent_impl<bireactor_class>::all_agent_calc_and_send_config_if_needed(
    const agent_config::pointer& new_conf) {
  absl::MutexLock l(&_instances_m);
  for (auto& instance : *_instances) {
    instance->calc_and_send_config_if_needed(new_conf);
  }
}

static bool add_command_to_agent_conf(
    const std::string& cmd_name,
    const std::string& cmd_line,
    const std::string& service,
    uint32_t check_interval,
    com::centreon::agent::AgentConfiguration* cnf,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& peer,
    bool encrypt_credentials) {
  std::string plugins_cmdline = boost::trim_copy(cmd_line);

  if (plugins_cmdline.empty()) {
    SPDLOG_LOGGER_ERROR(
        logger,
        "no add command: agent: {} serv: {}, no plugins cmd_line found in {}",
        peer, service, cmd_line);
    return false;
  }

  SPDLOG_LOGGER_TRACE(
      logger, "add command to agent: {}, serv: {}, cmd {} plugins cmd_line {}",
      peer, service, cmd_name, cmd_line);

  com::centreon::agent::Service* serv = cnf->add_services();
  serv->set_service_description(service);
  serv->set_command_name(cmd_name);
  if (encrypt_credentials && credentials_decrypt) {
    serv->set_command_line("encrypt::" +
                           credentials_decrypt->encrypt(plugins_cmdline));
  } else {
    serv->set_command_line(plugins_cmdline);
  }
  serv->set_check_interval(check_interval * pb_config.interval_length());

  return true;
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
      bool at_least_one_command_found = get_otel_commands(
          _agent_info->init().host(),
          [cnf, &peer, crypt_credentials](
              const std::string& cmd_name, const std::string& cmd_line,
              const std::string& service, uint32_t check_interval,
              const std::shared_ptr<spdlog::logger>& logger) {
            return add_command_to_agent_conf(cmd_name, cmd_line, service,
                                             check_interval, cnf, logger, peer,
                                             crypt_credentials);
          },
          _whitelist_cache, _logger);
      if (!at_least_one_command_found) {
        SPDLOG_LOGGER_ERROR(_logger, "no command found for agent {}",
                            get_peer());
      }
    }
    if (!_last_sent_config ||
        !::google::protobuf::util::MessageDifferencer::Equals(
            *cnf, _last_sent_config->config())) {
      _last_sent_config = new_conf;
    } else {
      new_conf.reset();
      SPDLOG_LOGGER_DEBUG(_logger, "no need to update conf to {}", get_peer());
    }
  }
  if (new_conf) {
    SPDLOG_LOGGER_DEBUG(_logger, "send conf to {}", get_peer());
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
  absl::MutexLock l(&_instances_m);
  _instances->insert(strm);
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
  asio::post(*_io_context,
             [me = std::enable_shared_from_this<
                  agent_impl<bireactor_class>>::shared_from_this(),
              logger = _logger]() {
               absl::MutexLock l(&_instances_m);
               SPDLOG_LOGGER_DEBUG(logger, "{:p} server::OnDone()",
                                   static_cast<void*>(me.get()));
               _instances->erase(
                   std::static_pointer_cast<agent_impl<bireactor_class>>(me));
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
      *_io_context, [me = std::enable_shared_from_this<
                         agent_impl<bireactor_class>>::shared_from_this(),
                     status, logger = _logger]() {
        absl::MutexLock l(&_instances_m);
        if (status.ok()) {
          SPDLOG_LOGGER_DEBUG(logger, "{:p} client::OnDone({}) {}",
                              static_cast<void*>(me.get()),
                              status.error_message(), status.error_details());
        } else {
          SPDLOG_LOGGER_ERROR(logger, "{:p} client::OnDone({}) {}",
                              static_cast<void*>(me.get()),
                              status.error_message(), status.error_details());
        }
        _instances->erase(
            std::static_pointer_cast<agent_impl<bireactor_class>>(me));
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
  std::set<std::shared_ptr<agent_impl>>* to_shutdown;
  {
    absl::MutexLock l(&_instances_m);
    to_shutdown = _instances;
    _instances = new std::set<std::shared_ptr<agent_impl<bireactor_class>>>;
  }
  for (std::shared_ptr<agent_impl> conn : *to_shutdown) {
    conn->shutdown();
  }
}

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

template class agent_impl<
    ::grpc::ClientBidiReactor<agent::MessageToAgent, agent::MessageFromAgent>>;

template class agent_impl<
    ::grpc::ServerBidiReactor<agent::MessageFromAgent, agent::MessageToAgent>>;

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent
