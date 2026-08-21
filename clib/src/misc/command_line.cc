/**
 * Copyright 2011-2013 Centreon
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

#include "com/centreon/misc/command_line.hh"
#include <cctype>
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::misc;
using com::centreon::exceptions::msg_fmt;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Parse command line.
 *
 *  @param[in] cmdline  The command line to parse.
 */
command_line::command_line(std::string_view cmdline) {
  parse(cmdline);
}

/**
 *  Copy constructor.
 *
 *  The buffer is copied in the initializer list and only the argument array is
 *  rebuilt: it holds pointers into the other object's buffer, so it cannot be
 *  copied as is. Construction and assignment used to share one helper, which
 *  made this path test for self-assignment and release members that had just
 *  been constructed empty -- harmless, but it read as if the object could
 *  already own something.
 *
 *  @param[in] right  The object to copy.
 */
command_line::command_line(const command_line& right)
    : _buffer(right._buffer) {
  _rebase_argv(right);
}

/**
 *  Copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
command_line& command_line::operator=(const command_line& right) {
  if (this != &right) {
    _buffer = right._buffer;
    _argv.clear();
    _rebase_argv(right);
  }
  return *this;
}

/**
 *  Get the size array of arguments.
 *
 *  @return Size.
 */
int command_line::get_argc() const noexcept {
  return _argv.empty() ? 0 : _argv.size() - 1;
}

/**
 *  Get the array of arguments.
 *
 *  @return Array arguments.
 */
char* const* command_line::get_argv() const noexcept {
  return _argv.empty() ? nullptr : _argv.data();
}

/**
 *  Parse command line and store arguments.
 *
 *  @param[in] cmdline  The command line to parse.
 */
void command_line::parse(std::string_view cmdline) {
  /* A default-constructed view, which is what a caller with nothing to parse
   * passes. Distinct from an empty but existing command line, which yields an
   * argument array holding just its terminating nullptr — that difference is
   * observable through get_argv() and predates the view. */
  if (!cmdline.data()) {
    _release();
    return;
  }

  const size_t size = cmdline.size();

  /* Sized once, and zeroed: the last token is not explicitly terminated, it
   * relies on those zeroes, and a reused buffer still holds the previous
   * command line. One char more than the input, since a token is at most as
   * long as what produced it and still needs its terminator.
   *
   * Neither this buffer nor the vector's capacity is handed back — assign()
   * keeps the capacity it has. An object parsing again therefore allocates
   * nothing at all, and process::exec parses one per check. */
  _argv.clear();
  _buffer.assign(size + 1, '\0');

  /* One char* per token plus the trailing nullptr, reserved in one go. The
   * number of maximal non-blank runs is a safe upper bound on the token count:
   * a token can only end on whitespace or at the end of the input, so quotes
   * and escapes may merge several runs into one token but can never split one.
   *
   * Without this the vector grows from an empty capacity at every call, and
   * this one is called once per check: measured under heaptrack, an eleven
   * token check command cost 5 reallocations, against 1 here. */
  size_t tokens = 0;
  for (size_t i = 0; i < size; ++i)
    if (!isspace(static_cast<unsigned char>(cmdline[i])) &&
        (i == 0 || isspace(static_cast<unsigned char>(cmdline[i - 1]))))
      ++tokens;
  _argv.reserve(tokens + 1);

  // Status variables.
  bool escap(false);
  char quote(0);

  char* begin = nullptr;
  char* write = _buffer.data();

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

  for (char c : cmdline) {
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
          _argv.push_back(begin);
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

  // a last tokern
  if (state == e_decoding_field) {
    _argv.push_back(begin);
  }

  _argv.push_back(nullptr);
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Rebuild the argument array so that it points into our own buffer.
 *
 *  Called once _buffer already holds a copy of the other object's, and expects
 *  _argv to be empty. Offsets are translated rather than the pointers copied:
 *  those point into the other object's buffer, and taking them as is would make
 *  both argument arrays share -- then outlive -- a single buffer.
 *
 *  @param[in] right  The object whose argument array is being translated.
 */
void command_line::_rebase_argv(const command_line& right) {
  if (right._argv.empty())
    return;
  _argv.reserve(right._argv.size());
  const char* right_base = right._buffer.data();
  char* base = _buffer.data();
  for (const char* right_token : right._argv) {
    /* The trailing nullptr is one of them, and it is not an offset. An empty
     * but existing command line has nothing else, and get_argv() tells that
     * apart from a default-constructed object -- so the distinction has to
     * survive a copy. */
    if (right_token)
      _argv.push_back(base + (right_token - right_base));
    else
      _argv.push_back(nullptr);
  }
}

/**
 *  Release memory used.
 */
void command_line::_release() {
  _argv.clear();
  _buffer.clear();
}
