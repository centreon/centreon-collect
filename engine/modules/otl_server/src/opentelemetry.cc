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

#include "com/centreon/exceptions/msg_fmt.hh"

#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/modules/otl_server/opentelemetry.hh"

#include "otl_fmt.hh"
#include "otl_server.hh"

using namespace com::centreon::engine::modules::otl_server;

open_telemetry::open_telemetry(const std::string_view config_file_path,
                               asio::io_context& io_context)
    : _config_file_path(config_file_path), _second_timer(io_context) {}

void open_telemetry::_reload() {
  std::unique_ptr<otl_config> new_conf =
      std::make_unique<otl_config>(_config_file_path);
  if (!_conf || !(*new_conf->get_grpc_config() == *_conf->get_grpc_config())) {
    std::shared_ptr<otl_server> to_shutdown = _otl_server.lock();
    if (to_shutdown) {
      to_shutdown->shutdown(std::chrono::seconds(10));
    }
    _otl_server = otl_server::load(
        new_conf->get_grpc_config(),
        [me = shared_from_this()](const metric_request_ptr& request) {
          me->_on_metric(request);
        });
  }
  if (!_conf || !(*_conf == *new_conf)) {
    fmt::formatter<::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>::max_length_log =
        new_conf->get_max_length_grpc_log();
    fmt::formatter<::opentelemetry::proto::collector::metrics::v1::
                       ExportMetricsServiceRequest>::json_grpc_format =
        new_conf->get_json_grpc_log();
    data_point_fifo::update_fifo_limit(new_conf->get_second_fifo_expiry(),
                                       new_conf->get_max_fifo_size());

    _conf = std::move(new_conf);
  }
}

std::shared_ptr<open_telemetry> open_telemetry::load(
    const std::string_view& config_path,
    asio::io_context& io_context) {
  if (!_instance) {
    _instance = std::make_shared<open_telemetry>(config_path, io_context);
    instance()->_reload();
    instance()->_start_second_timer();
  }
  return instance();
}

void open_telemetry::reload() {
  if (_instance) {
    try {
      instance()->_reload();
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(
          log_v2::otl(),
          "bad configuration, new configuration  not taken into account: {}",
          e.what());
    }
  }
}

void open_telemetry::unload() {
  if (_instance) {
    instance()->_shutdown();
    _instance.reset();
  }
}

void open_telemetry::_shutdown() {
  std::shared_ptr<otl_server> to_shutdown = _otl_server.lock();
  if (to_shutdown) {
    to_shutdown->shutdown(std::chrono::seconds(10));
  }
  std::lock_guard l(_protect);
  _second_timer.cancel();
}

std::shared_ptr<com::centreon::engine::commands::otel::host_serv_extractor>
open_telemetry::create_extractor(const std::string& cmdline) {
  // erase host serv extractors that are only owned by this object
  auto clean = [this]() {
    for (cmd_line_to_extractor_map::const_iterator to_test =
             _extractors.begin();
         !_extractors.empty() && to_test != _extractors.end();) {
      if (to_test->second.use_count() <= 1) {
        to_test = _extractors.erase(to_test);
      } else {
        ++to_test;
      }
    }
  };
  std::lock_guard l(_protect);
  auto exist = _extractors.find(cmdline);
  if (exist != _extractors.end()) {
    clean();
    return exist->second;
  }
  clean();
  std::shared_ptr<host_serv_extractor> new_extractor =
      host_serv_extractor::create(cmdline);
  _extractors.emplace(cmdline, new_extractor);
  return new_extractor;
}

bool open_telemetry::check(const std::string& processed_cmd,
                           uint64_t command_id,
                           nagios_macros& macros,
                           uint32_t timeout,
                           commands::result& res,
                           commands::otel::result_callback&& handler) {
  std::shared_ptr<otl_converter> to_use = otl_converter::create(
      processed_cmd, command_id, *macros.host_ptr, macros.service_ptr,
      std::chrono::system_clock::now() + std::chrono::seconds(timeout),
      std::move(handler));

  bool res_available = to_use->sync_build_result_from_metrics(_fifo, res);

  if (res_available) {
    return true;
  }

  // metrics not yet available = wait for data or until timeout
  std::lock_guard l(_protect);
  _waiting.insert(to_use);

  return false;
}

void open_telemetry::_on_metric(const metric_request_ptr& metrics) {
  std::vector<data_point> unknown;
  {
    std::lock_guard l(_protect);
    if (_extractors.empty()) {
      data_point::extract_data_points(metrics,
                                      [&unknown](const data_point& data_pt) {
                                        unknown.push_back(data_pt);
                                      });
    } else {
      waiting_converter::nth_index<0>::type& host_serv_index =
          _waiting.get<0>();
      std::vector<std::shared_ptr<otl_converter>> to_notify;
      auto last_success = _extractors.begin();
      data_point::extract_data_points(metrics, [this, &unknown, &last_success,
                                                &host_serv_index, &to_notify](
                                                   const data_point& data_pt) {
        bool data_point_known = false;
        for (unsigned tries = 0; tries < _extractors.size(); ++tries) {
          host_serv_metric hostservmetric =
              last_success->second->extract_host_serv_metric(data_pt);
          if (!hostservmetric.host.empty()) {
            _fifo.add_data_point(hostservmetric.host, hostservmetric.service,
                                 hostservmetric.metric, data_pt);

            auto waiting = host_serv_index.equal_range(
                host_serv{hostservmetric.host, hostservmetric.service});
            while (waiting.first != waiting.second) {
              to_notify.push_back(*waiting.first);
              waiting.first = host_serv_index.erase(waiting.first);
            }
            data_point_known = true;
            break;
          }
          if (last_success == _extractors.end()) {
            last_success = _extractors.begin();
          } else {
            ++last_success;
          }
        }
        if (!data_point_known) {
          unknown.push_back(data_pt);  // unknown metric => forward to broker
        }
      });
      // we wait that all request datas have been computed to give us more
      // chance of converter success
      for (auto to_callback : to_notify) {
        if (!to_callback->async_build_result_from_metrics(
                _fifo)) {  // not enough data => repush in _waiting
          _waiting.insert(to_callback);
        }
      }
    }
  }
  if (!unknown.empty()) {
    _forward_to_broker(unknown);
  }
}

void open_telemetry::_start_second_timer() {
  std::lock_guard l(_protect);
  _second_timer.expires_from_now(std::chrono::seconds());
  _second_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        if (err) {
          return;
        };
      });
}

void open_telemetry::_second_timer_handler() {
  {
    std::lock_guard l(_protect);
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    waiting_converter::nth_index<1>::type& expiry_index = _waiting.get<1>();
    while (!_waiting.empty()) {
      auto oldest = expiry_index.begin();
      if ((*oldest)->get_time_out() > now) {
        break;
      }
      expiry_index.erase(oldest);
    }
  }

  _start_second_timer();
}

void open_telemetry::_forward_to_broker(
    const std::vector<data_point>& unknown) {}
