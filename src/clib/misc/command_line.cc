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

#include <cctype>
#include <cstring>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/command_line.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::misc;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
command_line::command_line() : _argc(0), _argv(NULL), _size(0) {}

/**
 *  Parse command line.
 *
 *  @param[in] cmdline  The command line to parse.
 *  @param[in] size     The command line size, if size equal 0 parse
 *                      calculate the command line size.
 */
command_line::command_line(char const* cmdline, unsigned int size)
    : _argc(0), _argv(NULL), _size(0) {
  parse(cmdline, size);
}

/**
 *  Parse command line.
 *
 *  @param[in] cmdline  The command line to parse.
 */
command_line::command_line(std::string const& cmdline)
    : _argc(0), _argv(NULL), _size(0) {
  parse(cmdline);
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
command_line::command_line(command_line const& right) : _argv(NULL) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
command_line::~command_line() throw() { _release(); }

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
  return (_argc == right._argc && _size == right._size &&
          !memcmp(_argv[0], right._argv[0], _size));
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
int command_line::get_argc() const throw() { return (_argc); }

/**
 *  Get the array of arguments.
 *
 *  @return Array arguments.
 */
char** command_line::get_argv() const throw() { return (_argv); }

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
  char* str(new char[size + 1]);
  _size = 0;
  str[_size] = 0;

  // Status variables.
  bool escap(false);
  char sep(0);
  char last(0);
  for (unsigned int i(0); i < size; ++i) {
    // Current processed char.
    char c(cmdline[i]);

    // Is this an escaped character ?
    escap = (last == '\\' ? !escap : false);
    if (escap) {
      switch (c) {
        case 'n':
          c = '\n';
          break;
        case 'r':
          c = '\r';
          break;
        case 't':
          c = '\t';
          break;
        case 'a':
          c = '\a';
          break;
        case 'b':
          c = '\b';
          break;
        case 'v':
          c = '\v';
          break;
        case 'f':
          c = '\f';
          break;
          // default:
          //   if (c != '"' && c != '\'')
          //     str[_size++] = '\\';
          //   break ;
      }
    }

    // End of token.
    if (!sep && isspace(c) && !escap) {
      if (_size && last != '\\' && !isspace(last)) {
        str[_size++] = 0;
        ++_argc;
      }
    }
    // Quotes.
    else if (!escap && (c == '\'' || c == '"')) {
      if (!sep)
        sep = c;
      else if (sep == c)
        sep = 0;
      else if ((c != '\\') || escap)
        str[_size++] = c;
    }
    // Normal char (backslashes are used for escaping).
    else if ((c != '\\') || escap)
      str[_size++] = c;
    last = c;
  }

  // Not-terminated quote.
  if (sep) {
    delete[] str;
    throw basic_error("missing separator '{}'", sep);
  }

  // Terminate string if not already done so.
  if (last && _size && str[_size - 1]) {
    str[_size] = 0;
    ++_argc;
  }

  // Put tokens in table.
  _size = 0;
  _argv = new char* [_argc + 1];
  _argv[_argc] = NULL;
  for (int i(0); i < _argc; ++i) {
    _argv[i] = str + _size;
    while (str[_size++])
      ;
  }

  // If no token were found, avoid memory leak.
  if (!_argv[0])
    delete[] str;

  return;
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
    _argc = right._argc;
    _size = right._size;
    _release();
    if (right._argv) {
      _argv = new char* [_argc + 1];
      _argv[0] = new char[_size];
      _argv[_argc] = NULL;
      memcpy(_argv[0], right._argv[0], _size);
      unsigned int pos(0);
      for (int i(0); i < _argc; ++i) {
        _argv[i] = _argv[0] + pos;
        while (_argv[0][pos++])
          ;
      }
    }
  }
  return;
}

/**
 *  Release memory used.
 */
void command_line::_release() {
  if (_argv)
    delete[] _argv[0];
  delete[] _argv;
  _argv = NULL;
  return;
}
