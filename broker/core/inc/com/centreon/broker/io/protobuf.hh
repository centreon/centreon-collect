/**
 * Copyright 2021-2023 Centreon
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

#ifndef CCB_IO_PROTOBUF_HH
#define CCB_IO_PROTOBUF_HH

#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>

#include "com/centreon/broker/io/data.hh"
#include "com/centreon/broker/io/event_info.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::broker::io {
/**
 *  @class data protobuf.hh "com/centreon/broker/io/protobuf.hh"
 *  @brief Data abstraction.
 *
 *  Data is the core element that is transmitted through Centreon
 *  Broker. It is an interface that is implemented by all specific
 *  module data that wish to be transmitted by the multiplexing
 *  engine.
 *
 *  protobuf_base is directly inherited from data and is common to all
 *  bbdo protobuf messages. Its main goal is to allow the access to the
 *  embedded protobuf message as a google::protobuf::Message* pointer.
 *  We can access it through methods mut_msg() or msg() (mutable or not).
 *
 *  This class is very important when we want to parse a protobuf message
 *  without knowing about its exact type. This is very useful with reflection
 *  (see code in Lua module for more examples).
 */
class protobuf_base : public data {
  google::protobuf::Message* _msg;

 protected:
  protobuf_base(uint32_t typ, google::protobuf::Message* msg)
      : data(typ), _msg{msg} {}

  void set_message(google::protobuf::Message* msg) { _msg = msg; }

 public:
  enum attribute {
    always_valid = 0,
    invalid_on_zero = (1 << 0),
    invalid_on_minus_one = (1 << 1)
  };

  /**
   * @brief Accessor to the protobuf::Message* pointer as mutable.
   *
   * @return a google::protobuf::Message* pointer.
   */
  google::protobuf::Message* mut_msg() { return _msg; }

  /**
   * @brief Accessor to the protouf::Message* pointer.
   *
   * @return a google::protobuf::Message* pointer.
   */
  const google::protobuf::Message* msg() const { return _msg; }
};

/**
 * @class protobuf<T> protobuf.hh "com/centreon/broker/io/protobuf.hh"
 * @brief This is the template to use each time we have to embed a protobuf
 * message into a BBDO event.
 *
 * @tparam T A Protobuf message
 * @tparam Typ The type to associate to this class as a BBDO type.
 */
template <typename T, uint32_t Typ>
class protobuf : public protobuf_base {
  T _obj;

 public:
  using pb_type = T;
  using this_type = protobuf<T, Typ>;
  using shared_ptr = std::shared_ptr<this_type>;

  /**
   * @brief Default constructor
   */
  protobuf() : protobuf_base(Typ, &_obj) {}
  /**
   * @brief Constructor that initializes the T object from an existing one.
   * The io::protobuf class is a BBDO object that encapsulates a protobuf
   * object. It is useful to be able to construct an io::protobuf directly from
   * the Protobuf object.
   *
   * @param o The protobuf object (it is copied).
   */
  protobuf(const T& o) : protobuf_base(Typ, &_obj), _obj(o) {}

  /**
   * @brief Construct a new protobuf object
   * same as previous constructor except that it accepts two more parameters
   * @param o
   * @param src_id source_id
   * @param dest_id destination_id
   */
  protobuf(const T& o, uint32_t src_id, uint32_t dest_id) : protobuf(o) {
    source_id = src_id;
    destination_id = dest_id;
  }

  protobuf(const protobuf& to_clone) : protobuf_base(Typ, &_obj) {
    _obj.CopyFrom(to_clone._obj);
  }

  protobuf& operator=(const protobuf& to_clone) {
    _obj.CopyFrom(to_clone._obj);
    return *this;
  }

  ~protobuf() noexcept = default;

  bool operator==(const this_type& to_cmp) const;

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
    if (!r->obj().SerializeToString(&retval))
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
    auto retval = std::make_unique<protobuf<T, Typ>>();
    if (!retval->mut_msg()->ParseFromArray(buffer, size))
      throw com::centreon::exceptions::msg_fmt(
          "Unable to unserialize protobuf object");
    return retval.release();
  }

  virtual const T& obj() const { return _obj; }

  virtual T& mut_obj() { return _obj; }

  virtual void set_obj(T&& obj) { _obj = std::move(obj); }

  void dump(std::ostream& s) const override;
  void dump_more_detail(std::ostream& s) const override;
  void dump_to_json(std::ostream& s) const override;

  /**
   * @brief An internal BBDO object used to access to the constructor,
   * serialization and unserialization functions.
   */
  const static io::event_info::event_operations operations;
};

template <typename T, uint32_t Typ>
const io::event_info::event_operations protobuf<T, Typ>::operations{
    &new_proto, &serialize, &unserialize};

template <typename T, uint32_t Typ>
bool protobuf<T, Typ>::operator==(const protobuf<T, Typ>& to_cmp) const {
  return google::protobuf::util::MessageDifferencer::Equals(this->obj(),
                                                            (&to_cmp)->obj());
}

template <typename T, uint32_t Typ>
void protobuf<T, Typ>::dump(std::ostream& s) const {
  data::dump(s);
  std::string dump{this->obj().ShortDebugString()};
  if (dump.size() > 2000) {
    dump.resize(2000);
    s << fmt::format(" content:'{}...'", dump);
  } else
    s << " content:'" << dump << '\'';
}

template <typename T, uint32_t Typ>
void protobuf<T, Typ>::dump_more_detail(std::ostream& s) const {
  data::dump(s);
  s << " content:'" << this->obj().ShortDebugString() << '\'';
}

template <typename T, uint32_t Typ>
void protobuf<T, Typ>::dump_to_json(std::ostream& s) const {
  std::string json_dump;
  auto status =
      google::protobuf::util::MessageToJsonString(this->obj(), &json_dump);
  s << " content:'" << json_dump << '\'';
}

}  // namespace com::centreon::broker::io

#endif  // !CCB_IO_PROTOBUF_HH
