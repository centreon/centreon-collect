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
#include "common/crypto/aes256.hh"

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
 * @param unix_commandline
 */
process_args::process_args(const std::string_view& unix_commandline) {
  bool escap(false);
  char quote(0);

  std::string current;

  enum e_state { e_waiting_begin, e_decoding_field, e_decoding_in_quote };
  e_state state = e_waiting_begin;

  auto on_escape = [&](char c) {
    switch (c) {
      case 'n':
        current.push_back('\n');
        break;
      case 'r':
        current.push_back('\r');
        break;
      case 't':
        current.push_back('\t');
        break;
      case 'a':
        current.push_back('\a');
        break;
      case 'b':
        current.push_back('\b');
        break;
      case 'v':
        current.push_back('\v');
        break;
      case 'f':
        current.push_back('\f');
        break;
      default:
        current.push_back(c);
        break;
    }
    escap = false;
  };

  auto push_args = [&]() {
    if (_exe_path.empty()) {
      _exe_path = std::move(current);
    } else {
      _args.push_back(current);
    }
    current.clear();
  };

  for (char c : unix_commandline) {
    switch (state) {
      case e_waiting_begin:
        if (escap) {
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
          current.push_back(c);
        }
        break;
      case e_decoding_field:
        if (escap) {
          on_escape(c);
        } else if (c == '\\') {
          escap = true;
        } else if (isspace(c)) {  // field end
          push_args();
          state = e_waiting_begin;
        } else if (c == '"' || c == '\'') {
          state = e_decoding_in_quote;
          quote = c;
        } else {
          current.push_back(c);
        }
        break;
      case e_decoding_in_quote:
        if (escap) {
          on_escape(c);
        } else if (c == '\\') {
          escap = true;
        } else if (c == quote) {
          state = e_decoding_field;
        } else {
          current.push_back(c);
        }
        break;
    }
  }

  if (state == e_decoding_in_quote)
    throw msg_fmt("missing separator '{}'", quote);

  // a last tokern
  if (state == e_decoding_field) {
    push_args();
  }

  _c_args.reserve(_args.size() + 2);
  if (!_exe_path.empty()) {
    _c_args.push_back(_exe_path.c_str());
  }
  for (const std::string& arg : _args) {
    _c_args.push_back(arg.c_str());
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

/**
 * @brief encrypt all arguments, exe_path is not encrypted
 *
 * @param crypto
 */
void process_args::encrypt_args(const crypto::aes256& crypto) {
  _encrypted_args.reserve(_args.size());
  for (const std::string& s : _args) {
    _encrypted_args.push_back(crypto.encrypt(s));
  }
}

/**
 * @brief decrypt all arguments
 *
 * @param crypto
 */
void process_args::decrypt_args(const crypto::aes256& crypto) {
  auto decrypt_iter = _args.begin();
  auto c_args_iter = _c_args.begin();
  ++c_args_iter;  // exe
  for (const std::string& s : _encrypted_args) {
    crypto.decrypt(s, &*decrypt_iter);
    *c_args_iter = decrypt_iter->c_str();
    ++c_args_iter;
    ++decrypt_iter;
  }
}

/**
 * @brief clear unencrypted arguments
 *
 */
void process_args::clear_no_encrypted_args() {
  for (std::string& s : _args) {
    s.assign(s.size(), ' ');
    s.clear();
  }
}
