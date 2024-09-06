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
#include "bbdo/bbdo.pb.h"

namespace com::centreon::broker::config::applier {
using com::centreon::broker::PeerType;

/**
 * @brief This class represents a connected peer to this instance. It can be
 * an Engine, another Broker or a Map.
 */
class peer {
  PeerType _type;
  std::string _name;
  std::string _engine_configuration_version;
  bool _synchronized = false;

 public:
  peer() = default;
  /**
   * @brief Constructor of a peer.
   *
   * @param name The peer name
   * @param engine_configuration_version The Engine configuration version (only
   * if it is an Engine, empty otherwise).
   */
  peer(PeerType type,
       const std::string& name,
       const std::string engine_configuration_version)
      : _type{type},
        _name{name},
        _engine_configuration_version{engine_configuration_version} {}
  /**
   * @brief Name accessor
   *
   * @return A reference to the name.
   */
  const std::string& name() const { return _name; }
  void set_name(const std::string& name) { _name = name; }
  /**
   * @brief Engine configuration version accessor.
   *
   * @return A reference to the configuration version.
   */
  const std::string& engine_configuration_version() const {
    return _engine_configuration_version;
  }
  void set_engine_configuration_version(const std::string& version) {
    _engine_configuration_version = version;
  }
  /**
   * @brief Peer type accessor. It can be a value among INPUT, OUTPUT.
   *
   * @return the type.
   */
  PeerType type() const { return _type; }
  void set_type(PeerType type) { _type = type; }
  /**
   * @brief When Engine starts to send data to Broker, it can send data older
   * than its new configuration since we work with a retention mechanism.
   * Then an instance message is sent by Engine, and from that moment, messages
   * are based on the new configuration. So events are coherent with the Engine
   * configuration known by both. This is the instant when we consider them to
   * be synchronized. This method is used to change the _synchronized flag.
   *
   * @param synchronized A boolean.
   */
  void set_synchronized(bool synchronized) { _synchronized = synchronized; }
  /**
   * @brief When Engine starts to send data to Broker, it can send data older
   * than its new configuration since we work with a retention mechanism.
   * Then an instance message is sent by Engine, and from that moment, messages
   * are based on the new configuration. So events are coherent with the Engine
   * configuration known by both. This is the instant when we consider them to
   * be synchronized. This method sends True when they are synchronized,
   * messages are coherent with configurations.
   *
   * @return a boolean.
   */
  bool is_synchronized() const { return _synchronized; }
};

}  // namespace com::centreon::broker::config::applier
