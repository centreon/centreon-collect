/**
 * Copyright 2019 Centreon (https://www.centreon.com/)
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

#include <sys/stat.h>

#include <chrono>
#include <future>

#include <grpcpp/server_builder.h>

#include "com/centreon/engine/host.hh"

#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/enginerpc.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine;

/**
 * @brief Tell if a listen address is a Unix domain socket, written with the
 * gRPC URI scheme `unix:<path>`.
 *
 * @param address The rpc_listen_address value.
 *
 * @return true for a Unix domain socket, false for a host name or an IP.
 */
bool enginerpc::is_unix_socket(const std::string& address) {
  return address.compare(0, 5, "unix:") == 0;
}

/**
 * @brief Build the address given to gRPC: a Unix domain socket is used as is,
 * any other address gets the port.
 *
 * @param address The rpc_listen_address value.
 * @param port The TCP port, ignored for a Unix domain socket.
 *
 * @return The address the server listens on.
 */
std::string enginerpc::listening_address(const std::string& address,
                                         uint16_t port) {
  if (is_unix_socket(address))
    return address;
  return fmt::format("{}:{}", address, port);
}

/**
 * @brief Start the gRPC server. A Unix domain socket is created with mode
 * 0660: the API has no authentication, the file permissions are its only
 * access control.
 *
 * @param address The rpc_listen_address value.
 * @param port The TCP port, ignored for a Unix domain socket.
 */
enginerpc::enginerpc(const std::string& address, uint16_t port) {
  std::string server_address{listening_address(address, port)};
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&_service);
  if (is_unix_socket(address)) {
    // The mode comes from the umask at bind time: the socket never exists
    // with wider permissions.
    mode_t previous = umask(0117);
    _server = builder.BuildAndStart();
    umask(previous);
  } else
    _server = builder.BuildAndStart();
}

/**
 * @brief Tell if the server is started.
 *
 * @return false when gRPC could not listen on the address.
 */
bool enginerpc::listening() const {
  return _server != nullptr;
}

/**
 * @brief Stop the gRPC server. Called once the main loop has stopped, while
 * the handlers of the calls in flight still wait for tasks only that loop
 * executes: those tasks are run here until gRPC has finished its last call,
 * otherwise a client calling the API during the shutdown blocks it forever.
 */
void enginerpc::shutdown() {
  if (!_server)
    return;
  std::future<void> stopped = std::async(std::launch::async, [this] {
    _server->Shutdown(std::chrono::system_clock::now() +
                      std::chrono::seconds(2));
    _server->Wait();
  });
  while (stopped.wait_for(std::chrono::milliseconds(10)) !=
         std::future_status::ready)
    command_manager::instance().execute();
}
