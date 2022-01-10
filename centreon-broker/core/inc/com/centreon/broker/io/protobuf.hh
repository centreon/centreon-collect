/*
** Copyright 2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_IO_PROTOBUF_HH
#define CCB_IO_PROTOBUF_HH

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/event_info.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

CCB_BEGIN()

namespace io {
/**
 *  @class data data.hh "com/centreon/broker/io/data.hh"
 *  @brief Data abstraction.
 *
 *  Data is the core element that is transmitted through Centreon
 *  Broker. It is an interface that is implemented by all specific
 *  module data that wish to be transmitted by the multiplexing
 *  engine.
 */
template <typename T, uint32_t Typ>
class protobuf : public data {
 public:
  T obj;

  /**
   * @brief Default constructor
   */
  protobuf() : data(Typ) {}
  /**
   * @brief Constructor that initializes the T object from an existing one.
   * The io::protobuf class is a BBDO object that encapsulates a protobuf
   * object. It is useful to be able to construct an io::protobuf directly from
   * the Protobuf object.
   *
   * @param o The protobuf object (it is copied).
   */
  protobuf(const T& o) : data(Typ), obj(o) {}
  protobuf(const protobuf&) = delete;
  ~protobuf() noexcept = default;
  protobuf& operator=(const protobuf&) = delete;

  /**
   *  Get the type of this event.
   *
   *  @return  The event type.
   */
  constexpr static uint32_t static_type() { return Typ; }

  /**
   * @brief A static operator to instantiate an instance of this class.
   *
   * @return A new instance of this class.
   */
  static io::data* new_proto() { return new protobuf<T, Typ>(); }

  /**
   * @brief Serialization function of this object. Here we encapsulate a
   * protobuf message, so this method calls the protobuf mechanism.
   *
   * @param e The object to serialize.
   *
   * @return A string with the serialized object.
   */
  static std::string serialize(const io::data& e) {
    std::string retval;
    auto r = static_cast<const protobuf<T, Typ>*>(&e);
    if (!r->obj.SerializeToString(&retval))
      throw com::centreon::exceptions::msg_fmt(
          "Unable to serialize {:x} protobuf object", Typ);
    return retval;
  }

  /**
   * @brief Unserialization function of this object. Here from a const char*
   * pointer, we create an instance of this class.
   *
   * @param buffer The pointer to the char* array
   * @param size The size of the array.
   *
   * @return a pointer to the new object.
   */
  static io::data* unserialize(const char* buffer, size_t size) {
    std::unique_ptr<protobuf<T, Typ>> retval =
        std::make_unique<protobuf<T, Typ>>();
    if (!retval->obj.ParseFromArray(buffer, size))
      throw com::centreon::exceptions::msg_fmt(
          "Unable to unserialize protobuf object");
    return retval.release();
  }

  /**
   * @brief An internal BBDO object used to access to the constructor,
   * serialization and unserialization functions.
   */
  const static io::event_info::event_operations operations;
};

template <typename T, uint32_t Typ>
const io::event_info::event_operations protobuf<T, Typ>::operations{
    &new_proto, &serialize, &unserialize};
}  // namespace io

CCB_END()

#endif  // !CCB_IO_PROTOBUF_HH
