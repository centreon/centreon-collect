/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

/**
 * @file fmt_protobuf.hh
 * @brief {fmt} formatter specializations for protobuf messages.
 *
 * Including this header enables formatting any `google::protobuf::Message`
 * subclass directly with `fmt::format` / `SPDLOG_*` macros:
 *
 * @code
 * MyProto msg;
 * // Single-line (default):
 * std::string s = fmt::format("{}", msg);
 *
 * // Multiline (human-readable, indented):
 * using com::centreon::common::multiline_output;
 * std::string s = fmt::format("{}", multiline_output(msg));
 * @endcode
 */

#ifndef CCCM_FMT_PROTOBUF_HH
#define CCCM_FMT_PROTOBUF_HH

#include <google/protobuf/json/json.h>
#include <google/protobuf/message.h>
#include <limits>
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/text_format.h"

namespace com::centreon::common {

/**
 * @brief Adapts a back-insert iterator to the `ZeroCopyOutputStream` interface.
 *
 * Protobuf's `TextFormat::Printer` writes to a `ZeroCopyOutputStream`.  This
 * adapter bridges that interface to any output iterator (typically
 * `fmt::format_context::iterator`) so text can be written directly into an
 * `{fmt}` format buffer without an intermediate `std::string` allocation.
 *
 * Internally uses a 4 KiB stack buffer; data is flushed to the iterator on
 * each `Next()` call and in the destructor.
 *
 * @tparam back_iterator  An output iterator type compatible with
 *                        `std::copy_n` (e.g. `fmt::format_context::iterator`).
 */
template <class back_iterator>
class back_iterator_output_stream
    : public ::google::protobuf::io::ZeroCopyOutputStream {
  back_iterator& _to_write;

  char _buf[4096];
  int64_t _buffered = 0;
  int64_t _total = 0;
  int64_t _max_length;

 public:
  explicit back_iterator_output_stream(
      back_iterator& to_write,
      int64_t max_length = std::numeric_limits<int64_t>::max())
      : _to_write(to_write), _max_length(max_length) {
    if (max_length < 0) {
      _max_length = std::numeric_limits<int64_t>::max();
    }
  }

  ~back_iterator_output_stream() { flush(); }

  /// Flushes pending bytes from the internal buffer to the iterator.
  void flush() {
    if (_buffered > 0 && _total < _max_length) {
      int64_t nb_to_write = _buffered;
      if (_total + nb_to_write > _max_length) {
        nb_to_write = _max_length - _total;
      }
      _to_write = std::copy_n(_buf, nb_to_write, _to_write);
      _total += _buffered;
      _buffered = 0;
    }
  }

  bool Next(void** data, int* size) override {
    flush();
    *data = _buf;
    *size = _buffered = sizeof(_buf);
    return true;
  }

  void BackUp(int count) override { _buffered -= count; }
  int64_t ByteCount() const override { return _total + _buffered; }
};

/**
 * @brief Wrapper requesting multiline text rendering of a protobuf message.
 *
 * Pass the result of `multiline_output(msg)` to `fmt::format` to get
 * indented, human-readable protobuf text format instead of the default
 * single-line form.
 */
struct multiline_output_struct {
  const ::google::protobuf::Message& to_print;
};

/**
 * @brief Creates a `multiline_output_struct` for `msg`.
 *
 * Convenience factory; deduces the concrete message type so callers do not
 * need to name `multiline_output_struct` explicitly.
 */
template <class protobuf_mess_type>
multiline_output_struct multiline_output(const protobuf_mess_type& mess) {
  return multiline_output_struct{mess};
}

};  // namespace com::centreon::common

namespace fmt {

/**
 * @brief {fmt} formatter for any `google::protobuf::Message` subclass.
 *
 * Renders the message as a **single-line json**.
 * No format specifiers are supported; `{}` is the only valid placeholder.
 *
 * Enabled only when `protobuf_mess_type` derives from
 * `google::protobuf::Message` (SFINAE guard via `enable_if_t`).
 */
template <class protobuf_mess_type>
struct formatter<protobuf_mess_type,
                 char,
                 std::enable_if_t<std::is_base_of_v<::google::protobuf::Message,
                                                    protobuf_mess_type>>> {
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const protobuf_mess_type& mess,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = ctx.out();
    com::centreon::common::back_iterator_output_stream<
        fmt::format_context::iterator>
        output_stream(out);
    [[maybe_unused]] auto ignored =
        ::google::protobuf::json::MessageToJsonStream(mess, &output_stream);
    return out;
  }
};

/**
 * @brief {fmt} formatter for `multiline_output_struct`.
 *
 * Renders the wrapped message as a **multiline** protobuf text format string
 * (fields on separate lines, nested messages indented).  Obtain a
 * `multiline_output_struct` via `com::centreon::common::multiline_output()`.
 */
template <>
struct formatter<com::centreon::common::multiline_output_struct> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const com::centreon::common::multiline_output_struct& mess,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = ctx.out();
    com::centreon::common::back_iterator_output_stream<
        fmt::format_context::iterator>
        output_stream(out);
    google::protobuf::TextFormat::Printer printer;
    printer.Print(mess.to_print, &output_stream);
    return out;
  }
};

}  // namespace fmt

#endif
