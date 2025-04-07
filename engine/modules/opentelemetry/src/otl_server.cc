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

#include <grpc/grpc.h>
#include <grpcpp/impl/codegen/server_callback.h>

#include "centreon_agent/agent.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"

#include "otl_fmt.hh"
#include "otl_server.hh"

namespace otl_col_metrics = ::opentelemetry::proto::collector::metrics::v1;
using namespace com::centreon::engine::modules::opentelemetry;

namespace com::centreon::engine::modules::opentelemetry::detail {

class request_response_allocator;
/**
 * @brief the goal of this class is  to work with shared_ptr<request>
 *
 */
class request_response_holder
    : public ::grpc::MessageHolder<
          otl_col_metrics::ExportMetricsServiceRequest,
          otl_col_metrics::ExportMetricsServiceResponse> {
  metric_request_ptr _request;
  otl_col_metrics::ExportMetricsServiceResponse _response;
  std::weak_ptr<request_response_allocator> _allocator;

 public:
  request_response_holder(
      const std::shared_ptr<request_response_allocator>& allocator)
      : _request(
            std::make_shared<otl_col_metrics::ExportMetricsServiceRequest>()),
        _allocator(allocator) {
    set_request(_request.get());
    set_response(&_response);
  }

  const metric_request_ptr& get_request() const { return _request; }

  void Release() override;
};

/**
 * @brief this allocator creates request_response_holder and stores request
 * shared pointers
 *
 */
class request_response_allocator
    : public ::grpc::MessageAllocator<
          otl_col_metrics::ExportMetricsServiceRequest,
          otl_col_metrics::ExportMetricsServiceResponse>,
      public std::enable_shared_from_this<request_response_allocator> {
  absl::btree_map<const otl_col_metrics::ExportMetricsServiceRequest*,
                  metric_request_ptr>
      _allocated;
  absl::Mutex _protect;

 public:
  metric_request_ptr get_metric_request_ptr_from_raw(
      const otl_col_metrics::ExportMetricsServiceRequest* raw_request);

  void release_request(
      const otl_col_metrics::ExportMetricsServiceRequest* raw_request);

  ::grpc::MessageHolder<otl_col_metrics::ExportMetricsServiceRequest,
                        otl_col_metrics::ExportMetricsServiceResponse>*
  AllocateMessages() override;
};

/**
 * @brief grpc layers work with raw pointer, this method return shared_ptr from
 * raw pointer
 *
 * @param raw_request request raw pointer
 * @return metric_request_ptr shared_ptr that holds raw_request
 */
metric_request_ptr request_response_allocator::get_metric_request_ptr_from_raw(
    const otl_col_metrics::ExportMetricsServiceRequest* raw_request) {
  absl::MutexLock l(&_protect);
  auto found = _allocated.find(raw_request);
  if (found != _allocated.end()) {
    return found->second;
  }
  return metric_request_ptr();
}

/**
 * @brief this objects stores relationship between raw pointer and shared_ptr.
 * This method releases a relationship
 *
 * @param raw_request
 */
void request_response_allocator::release_request(
    const otl_col_metrics::ExportMetricsServiceRequest* raw_request) {
  absl::MutexLock l(&_protect);
  _allocated.erase(raw_request);
}

/**
 * @brief allocate a request response pair and store relationship between raw
 * request pointer and shared_ptr
 *
 * @return ::grpc::MessageHolder<otl_col_metrics::ExportMetricsServiceRequest,
 * otl_col_metrics::ExportMetricsServiceResponse>*
 */
::grpc::MessageHolder<otl_col_metrics::ExportMetricsServiceRequest,
                      otl_col_metrics::ExportMetricsServiceResponse>*
request_response_allocator::AllocateMessages() {
  request_response_holder* ret =
      new request_response_holder(shared_from_this());
  absl::MutexLock l(&_protect);
  _allocated.emplace(ret->request(), ret->get_request());
  return ret;
}

/**
 * @brief destructor of request_response_holder called by grpc layers
 * it also call request_response_allocator::release_request
 *
 */
void request_response_holder::Release() {
  auto alloc = _allocator.lock();
  if (alloc)
    alloc->release_request(request());
  delete this;
}

/**
 * @brief grpc metric service
 *  we make a custom WithCallbackMethod_Export in order to use a shared ptr
 * instead a raw pointer
 */
class metric_service : public ::opentelemetry::proto::collector::metrics::v1::
                           MetricsService::Service,
                       public std::enable_shared_from_this<metric_service> {
  std::shared_ptr<request_response_allocator> _allocator;
  metric_handler _request_handler;
  std::shared_ptr<spdlog::logger> _logger;

  void init();

  void SetMessageAllocatorFor_Export(
      ::grpc::MessageAllocator<::opentelemetry::proto::collector::metrics::v1::
                                   ExportMetricsServiceRequest,
                               ::opentelemetry::proto::collector::metrics::v1::
                                   ExportMetricsServiceResponse>* allocator) {
    ::grpc::internal::MethodHandler* const handler =
        ::grpc::Service::GetHandler(0);
    static_cast<::grpc::internal::CallbackUnaryHandler<
        ::opentelemetry::proto::collector::metrics::v1::
            ExportMetricsServiceRequest,
        ::opentelemetry::proto::collector::metrics::v1::
            ExportMetricsServiceResponse>*>(handler)
        ->SetMessageAllocator(allocator);
  }

 public:
  metric_service(const metric_handler& request_handler,
                 const std::shared_ptr<spdlog::logger>& logger);

  static std::shared_ptr<metric_service> load(
      const metric_handler& request_handler,
      const std::shared_ptr<spdlog::logger>& logger);

  // disable synchronous version of this method
  ::grpc::Status Export(
      ::grpc::ServerContext* /*context*/,
      const ::opentelemetry::proto::collector::metrics::v1::
          ExportMetricsServiceRequest* /*request*/,
      ::opentelemetry::proto::collector::metrics::v1::
          ExportMetricsServiceResponse* /*response*/) override {
    abort();
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
  }
  virtual ::grpc::ServerUnaryReactor* Export(
      ::grpc::CallbackServerContext* /*context*/,
      const ::opentelemetry::proto::collector::metrics::v1::
          ExportMetricsServiceRequest* /*request*/,
      ::opentelemetry::proto::collector::metrics::v1::
          ExportMetricsServiceResponse* /*response*/);
};

/**
 * @brief Construct a new metric service::metric service object
 * this constructor must not be called by other objects, use load  instead
 *
 * @param request_handler
 */
metric_service::metric_service(const metric_handler& request_handler,
                               const std::shared_ptr<spdlog::logger>& logger)
    : _allocator(std::make_shared<request_response_allocator>()),
      _request_handler(request_handler),
      _logger(logger) {}

/**
 * @brief initialize service (set allocator and push callback)
 *
 */
void metric_service::init() {
  ::grpc::Service::MarkMethodCallback(
      0, new ::grpc::internal::CallbackUnaryHandler<
             ::opentelemetry::proto::collector::metrics::v1::
                 ExportMetricsServiceRequest,
             ::opentelemetry::proto::collector::metrics::v1::
                 ExportMetricsServiceResponse>(
             [me = shared_from_this()](
                 ::grpc::CallbackServerContext* context,
                 const ::opentelemetry::proto::collector::metrics::v1::
                     ExportMetricsServiceRequest* request,
                 ::opentelemetry::proto::collector::metrics::v1::
                     ExportMetricsServiceResponse* response) {
               return me->Export(context, request, response);
             }));
  SetMessageAllocatorFor_Export(_allocator.get());
}

/**
 * @brief the method to use to create a metric_service
 *
 * @param request_handler
 * @return std::shared_ptr<metric_service>
 */
std::shared_ptr<metric_service> metric_service::load(
    const metric_handler& request_handler,
    const std::shared_ptr<spdlog::logger>& logger) {
  std::shared_ptr<metric_service> ret =
      std::make_shared<metric_service>(request_handler, logger);
  ret->init();
  return ret;
}

/**
 * @brief called on request
 *
 * @param context
 * @param request
 * @param response
 * @return ::grpc::ServerUnaryReactor*
 */
::grpc::ServerUnaryReactor* metric_service::Export(
    ::grpc::CallbackServerContext* context,
    const otl_col_metrics::ExportMetricsServiceRequest* request,
    otl_col_metrics::ExportMetricsServiceResponse* response [[maybe_unused]]) {
  metric_request_ptr shared_request =
      _allocator->get_metric_request_ptr_from_raw(request);

  SPDLOG_LOGGER_TRACE(_logger, "receive:{}", *request);
  if (shared_request) {
    _request_handler(shared_request);
  } else {
    SPDLOG_LOGGER_ERROR(_logger, " unknown raw pointer {:p}",
                        static_cast<const void*>(request));
  }

  ::grpc::ServerUnaryReactor* reactor = context->DefaultReactor();
  reactor->Finish(::grpc::Status::OK);
  return reactor;
}

}  // namespace com::centreon::engine::modules::opentelemetry::detail

/**
 * @brief Construct a new otl server::otl server object
 *
 * @param conf grpc configuration
 * @param handler handler that will be called on every request
 */
otl_server::otl_server(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const grpc_config::pointer& conf,
    const centreon_agent::agent_config::pointer& agent_config,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    const centreon_agent::agent_stat::pointer& agent_stats)

    : common::grpc::grpc_server_base(conf, logger),
      _service(detail::metric_service::load(handler, logger)),
      _agent_service(
          centreon_agent::agent_service::load(io_context,
                                              agent_config,
                                              handler,
                                              logger,
                                              agent_stats,
                                              conf->get_trusted_tokens())) {}

/**
 * @brief Destroy the otl server::otl server object
 *
 */
otl_server::~otl_server() {
  shutdown(std::chrono::seconds(30));
}

/**
 * @brief create and start a otl_server
 *
 * @param conf
 * @param handler
 * @return otl_server::pointer otl_server started
 */
otl_server::pointer otl_server::load(
    const std::shared_ptr<boost::asio::io_context>& io_context,
    const grpc_config::pointer& conf,
    const centreon_agent::agent_config::pointer& agent_config,
    const metric_handler& handler,
    const std::shared_ptr<spdlog::logger>& logger,
    const centreon_agent::agent_stat::pointer& agent_stats) {
  otl_server::pointer ret(new otl_server(io_context, conf, agent_config,
                                         handler, logger, agent_stats));
  ret->start();
  return ret;
}

/**
 * @brief create and start a grpc server
 *
 */
void otl_server::start() {
  _init([this](::grpc::ServerBuilder& builder) {
    builder.RegisterService(_service.get());
    builder.RegisterService(_agent_service.get());
  });
}

/**
 * @brief update conf used by service to create
 *
 * @param agent_config
 */
void otl_server::update_agent_config(
    const centreon_agent::agent_config::pointer& agent_config) {
  _agent_service->update(agent_config);
}
