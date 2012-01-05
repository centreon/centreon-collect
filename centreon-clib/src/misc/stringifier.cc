/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::misc;

#ifdef _WIN32
// Standards ? Like C99 ? What for ?
#  define snprintf _snprintf
#endif // Win32

/**
 *  Default constructor.
 *
 *  @param[in] buffer  Default buffer.
 */
stringifier::stringifier(char const* buffer) throw ()
  : _buffer(_static_buffer),
    _current(0),
    _size(_static_buffer_size) {
  reset();
  if (buffer)
    *this << buffer;
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  Object to copy.
 */
stringifier::stringifier(stringifier const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
stringifier::~stringifier() throw () {
  if (_static_buffer != _buffer)
    delete[] _buffer;
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The Object to copy.
 *
 *  @return This object.
 */
stringifier& stringifier::operator=(stringifier const& right) {
  return (_internal_copy(right));
}

/**
 *  Insertion operator.
 *
 *  @param[in] b  Boolean to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(bool b) throw () {
  return (_insert("%s", b ? "true" : "false"));
}

/**
 *  Insertion operator.
 *
 *  @param[in] str  String to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(char const* str) throw () {
  if (!str)
    str = "(null)";
  return (_insert("%s", str));
}

/**
 *  Insertion operator.
 *
 *  @param[in] c  Char to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(char c) throw () {
  return (_insert("%c", c));
}

/**
 *  Insertion operator.
 *
 *  @param[in] d  Double to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(double d) throw () {
  return (_insert("%f", d));
}

/**
 *  Insertion operator.
 *
 *  @param[in] i  Integer to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(int i) throw () {
  return (_insert("%d", i));
}

/**
 *  Insertion operator.
 *
 *  @param[in] ll Long long to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(long long ll) throw () {
  return (_insert("%lld", ll));
}

/**
 *  Insertion operator.
 *
 *  @param[in] l  Long to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(long l) throw () {
  return (_insert("%ld", l));
}

/**
 *  Insertion operator.
 *
 *  @param[in] str  String to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(std::string const& str) throw () {
  return (_insert("%s", str.c_str()));
}

/**
 *  Insertion operator.
 *
 *  @param[in] str  Stringifier to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(stringifier const& str) throw () {
  return (_insert("%.*s", str.size(), str.data()));
}

/**
 *  Insertion operator.
 *
 *  @param[in] u  Unsigned integer to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(unsigned int u) throw () {
  return (_insert("%u", u));
}

/**
 *  Insertion operator.
 *
 *  @param[in] ull  Unsigned long long to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(unsigned long long ull) throw () {
  return (_insert("%llu", ull));
}

/**
 *  Insertion operator.
 *
 *  @param[in] ul  Unsigned long to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(unsigned long ul) throw () {
  return (_insert("%lu", ul));
}

/**
 *  Insertion operator.
 *
 *  @param[in] p  Pointer address to concatenate to basic message.
 *
 *  @return This object.
 */
stringifier& stringifier::operator<<(void const* p) throw () {
  return (_insert("%p", p));
}

/**
 *  Adding an additional string at its end.
 *
 *  @param[in] str   The string to add.
 *  @param[in] size  The size to add.
 *
 *  @return This object
 */
stringifier& stringifier::append(
                            char const* str,
                            unsigned int size) throw () {
  return (_insert("%.*s", size, str));
}

/**
 *  Get C-String style data.
 *
 *  @return The pointer on data.
 */
char const* stringifier::data() const throw () {
  return (_buffer);
}

/**
 *  Reset the internal buffer to the empty string.
 */
void stringifier::reset() throw () {
  _buffer[0] = 0;
  _current = 0;
}

/**
 *  Get the current string size.
 *
 *  @return The size of the buffer.
 */
unsigned int stringifier::size() const throw () {
  return (_current);
}

/**
 *  Insert data into the current buffer.
 *
 *  @param[in] format  Specifies how the next arguments is converted
 *                     for output.
 *  @param[in] val     The object to convert.
 *
 *  @return This object.
 */
template <typename T>
stringifier& stringifier::_insert(
                            char const* format,
                            T val) throw () {
  int ret(snprintf(_buffer + _current,
                   _size - _current,
                   format,
                   val));
  if (ret < 0)
    return (*this);

  unsigned int size(static_cast<unsigned int>(ret + 1));
  if (size + _current > _size) {
    if (!_realloc(size))
      return (*this);
    if ((ret = snprintf(_buffer + _current,
                        _size - _current,
                        format,
                        val)) < 0)
      return (*this);
  }
  _current += ret;
  return (*this);
}

/**
 *  Insert data into the current buffer with size limit.
 *
 *  @param[in] format  Specifies how the next arguments is converted
 *                     for output.
 *  @param[in] limit   The size limit.
 *  @param[in] val     The object to convert.
 *
 *  @return This object.
 */
template <typename T>
stringifier& stringifier::_insert(
                            char const* format,
                            unsigned int limit,
                            T val) throw () {
  int ret(snprintf(_buffer + _current,
                   _size - _current,
                   format,
                   limit,
                   val));
  if (ret < 0)
    return (*this);

  unsigned int size(static_cast<unsigned int>(ret + 1));
  if (size + _current > _size) {
    if (!_realloc(size + _current))
      return (*this);
    if ((ret = snprintf(_buffer + _current,
                        _size - _current,
                        format,
                        limit,
                        val)) < 0)
      return (*this);
  }
  _current += ret;
  return (*this);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
stringifier& stringifier::_internal_copy(stringifier const& right) {
  if (this != &right) {
    if (right._size > _size) {
      if (_static_buffer != _buffer)
        delete[] _buffer;
      _buffer = new char[right._size];
    }
    _size = right._size;
    _current = right._current;
    memcpy(_buffer, right._buffer, (_current + 1) * sizeof(*_buffer));
  }
  return (*this);
}

/**
 *  Memory reallocation.
 *
 *  @param[in] new_size  The new memory size.
 *
 *  @return True on success, otherwise false.
 */
bool stringifier::_realloc(unsigned int new_size) throw () {
  try {
    _size = (new_size > _size * 2 ? new_size : _size * 2);
    char* new_buffer(new char[_size]);
    memcpy(new_buffer, _buffer, (_current + 1) * sizeof(*new_buffer));
    if (_static_buffer != _buffer)
      delete[] _buffer;
    _buffer = new_buffer;
  }
  catch (...) {
    return (false);
  }
  return (true);
}
