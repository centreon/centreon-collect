/**
 * Copyright 2025 Centreon
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

#include "com/centreon/common/process/process_args.hh"
#include <stdexcept>

using namespace com::centreon::common;
using com::centreon::exceptions::msg_fmt;

/**
 * @brief Construct a new process args::process args object
 *
 * @param exe_path first field of cmdline
 * @param args following arguments
 */
process_args::process_args(const std::string_view& exe_path,
                           std::vector<std::string>&& args)
    : _exe_path(exe_path), _args(args) {
  _c_args.reserve(_args.size() + 2);
  _c_args.push_back(_exe_path.c_str());
  for (const std::string& arg : _args) {
    _c_args.push_back(arg.c_str());
  }
  _c_args.push_back(nullptr);
}

/**
 * @brief Construct a new process args::process args object with an unix style
 * commandline
 *
 * Caution, as this is unix case, get_args() will return an empty vector, only
 * get_c_args() will contain arguments of command
 * In that case only _buffer is used and _c_args point to _buffer
 *
 * @param unix_commandline
 */
process_args::process_args(const std::string_view& unix_commandline) {
  _buffer = std::make_unique<char[]>(unix_commandline.length() + 1);

  memcpy(_buffer.get(), unix_commandline.data(), unix_commandline.length());
  _buffer.get()[unix_commandline.length()] = '\0';
  // Status variables.shared_ptr
  bool escap(false);
  char quote(0);

  char* begin = nullptr;
  char* write = _buffer.get();

  enum e_state { e_waiting_begin, e_decoding_field, e_decoding_in_quote };
  e_state state = e_waiting_begin;

  auto on_escape = [&](char c) {
    switch (c) {
      case 'n':
        *(write++) = '\n';
        break;
      case 'r':
        *(write++) = '\r';
        break;
      case 't':
        *(write++) = '\t';
        break;
      case 'a':
        *(write++) = '\a';
        break;
      case 'b':
        *(write++) = '\b';
        break;
      case 'v':
        *(write++) = '\v';
        break;
      case 'f':
        *(write++) = '\f';
        break;
      default:
        *(write++) = c;
        break;
    }
    escap = false;
  };

  for (char c : unix_commandline) {
    switch (state) {
      case e_waiting_begin:
        if (escap) {
          begin = write;
          on_escape(c);
          state = e_decoding_field;
        } else if (c == '\\') {
          escap = true;
        } else if (c == '"' || c == '\'') {
          state = e_decoding_in_quote;
          quote = c;
        } else if (isspace(c)) {
          continue;
        } else {
          state = e_decoding_field;
          begin = write;
          *(write++) = c;
        }
        break;
      case e_decoding_field:
        if (escap) {
          on_escape(c);
        } else if (c == '\\') {
          escap = true;
        } else if (isspace(c)) {  // field end
          *(write++) = 0;
          _c_args.push_back(begin);
          begin = nullptr;
          state = e_waiting_begin;
        } else if (c == '"' || c == '\'') {
          state = e_decoding_in_quote;
          quote = c;
        } else {
          *(write++) = c;
        }
        break;
      case e_decoding_in_quote:
        if (escap) {
          on_escape(c);
        } else if (c == '\\') {
          escap = true;
        } else if (c == quote) {
          if (!begin) {  // empty string between quotes
            begin = write;
          }
          state = e_decoding_field;
        } else {
          if (!begin)
            begin = write;
          *(write++) = c;
        }
        break;
    }
  }

  if (state == e_decoding_in_quote)
    throw msg_fmt("missing separator '{}'", quote);

  *write = '\0';
  // a last tokern
  if (state == e_decoding_field) {
    _c_args.push_back(begin);
  }

  if (_c_args.empty()) {
    throw std::invalid_argument("empty command line");
  }

  _c_args.push_back(nullptr);
}

void process_args::dump(std::string* output) const {
  output->reserve(1024);
  output->push_back('[');
  for (const char* arg : _c_args) {
    if (arg) {
      output->push_back('"');
      output->append(arg);
      output->append("\", ");
    }
  }
  output->push_back(']');
}
