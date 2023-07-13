/*
** Copyright 2011-2013 Centreon
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

#include "com/centreon/misc/command_line.hh"
#include <cctype>
#include <cstring>
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::misc;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
command_line::command_line() : _buffer(nullptr), _size(0) {}

/**
 *  Parse command line.
 *
 *  @param[in] cmdline  The command line to parse.
 *  @param[in] size     The command line size, if size equal 0 parse
 *                      calculate the command line size.
 */
command_line::command_line(char const* cmdline, unsigned int size)
    : _buffer(nullptr), _size(0) {
  parse(cmdline, size);
}

/**
 *  Parse command line.
 *
 *  @param[in] cmdline  The command line to parse.
 */
command_line::command_line(std::string const& cmdline)
    : _buffer(nullptr), _size(0) {
  parse(cmdline);
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
command_line::command_line(command_line const& right)
    : _buffer(nullptr), _size(0) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
command_line::~command_line() throw() {
  _release();
}

/**
 *  Copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
command_line& command_line::operator=(command_line const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if objects are equal, otherwise false.
 */
bool command_line::operator==(command_line const& right) const throw() {
  return (_size == right._size && !memcmp(_buffer, right._buffer, _size));
}

/**
 *  Not equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if objects are not equal, otherwise false.
 */
bool command_line::operator!=(command_line const& right) const throw() {
  return (!operator==(right));
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
 *  @param[in] size     The command line size, if size equal 0 parse
 *                      calculate the command line size.
 */
void command_line::parse(char const* cmdline, unsigned int size) {
  // Cleanup.
  _release();

  if (!cmdline)
    return;

  if (!size)
    size = strlen(cmdline);

  // Allocate buffer.
  _size = size + 1;
  _buffer = new char[_size];
  memset(_buffer, 0, _size);

  // Status variables.
  bool escap(false);
  char quote(0);

  char* begin = nullptr;
  char* write = _buffer;

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

  for (const char *current = cmdline, *end = cmdline + size; current < end;
       ++current) {
    // Current processed char.
    char c(*current);

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
    throw(basic_error() << "missing separator '" << quote << "'");

  // a last tokern
  if (state == e_decoding_field) {
    _argv.push_back(begin);
  }

  _argv.push_back(nullptr);
}

/**
 *  Parse command line and store arguments.
 *
 *  @param[in] cmdline  The command line to parse.
 */
void command_line::parse(std::string const& cmdline) {
  parse(cmdline.c_str(), cmdline.size());
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 */
void command_line::_internal_copy(command_line const& right) {
  if (this != &right) {
    _release();
    _size = right._size;
    if (right._buffer) {
      _buffer = new char[_size];
      memcpy(_buffer, right._buffer, _size);
      for (const char* right_token : right._argv) {
        if (right_token)
          _argv.push_back(_buffer + (right_token - right._buffer));
        else
          _argv.push_back(nullptr);
      }
    }
  }
}

/**
 *  Release memory used.
 */
void command_line::_release() {
  _argv.clear();
  delete[] _buffer;
  _buffer = nullptr;
  _size = 0;
}
