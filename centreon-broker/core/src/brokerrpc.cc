/*
 * Copyright 2019-2021 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/brokerrpc.hh"
#include <fmt/format.h>
#include <grpcpp/server_builder.h>
#include "bbdo/events.hh"
#include "com/centreon/broker/io/events.hh"

using namespace com::centreon::broker;

/**
 * @brief Constructor of our gRPC server
 *
 * @param address
 * @param port
 * @param broker_name
 */
brokerrpc::brokerrpc(const std::string& address,
                     uint16_t port,
                     std::string const& broker_name) {
  io::events& e{io::events::instance()};

  /* Lets' register the rebuild_metrics bbdo event. This is needed to send the
   * rebuild message. */
  e.register_event(make_type(io::bbdo, bbdo::de_rebuild_metrics),
                   "rebuild_metrics", &bbdo::pb_rebuild_metrics::operations);

  _service.set_broker_name(broker_name);
  std::string server_address{fmt::format("{}:{}", address, port)};
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&_service);
  _server = builder.BuildAndStart();
}

/**
 * @brief The shutdown() method, to stop the gRPC server.
 */
void brokerrpc::shutdown() {
  _server->Shutdown();
  _server->Wait();
}
