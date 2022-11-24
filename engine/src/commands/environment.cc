/*
** Copyright 2012-2013 Merethis
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

#include "com/centreon/engine/commands/environment.hh"
#include "com/centreon/engine/exceptions/error.hh"

using namespace com::centreon::engine::commands;

// Default size.
static uint32_t const EXTRA_SIZE_ENV = 128;
static uint32_t const EXTRA_SIZE_BUFFER = 4096;

/**
 *  Destructor.
 */
environment::~environment() noexcept {
  delete[] _buffer;
  delete[] _env;
}

/**
 *  Add an environment variable.
 *
 *  @param[in] name   The name of the environment variable.
 *  @param[in] value  The environment variable value.
 */
void environment::add(char const* name, char const* value) {
  if (!name || !value)
    return;
  uint32_t size_name(strlen(name));
  uint32_t size_value(strlen(value));
  uint32_t new_pos(_pos_buffer + size_name + size_value + 2);
  if (new_pos > _size_buffer) {
    if (new_pos < _size_buffer + EXTRA_SIZE_BUFFER)
      _realloc_buffer(_size_buffer + EXTRA_SIZE_BUFFER);
    else
      _realloc_buffer(new_pos + EXTRA_SIZE_BUFFER);
  }
  memcpy(_buffer + _pos_buffer, name, size_name + 1);
  _buffer[_pos_buffer + size_name] = '=';
  memcpy(_buffer + _pos_buffer + size_name + 1, value, size_value + 1);
  if (_pos_env + 1 >= _size_env)
    _realloc_env(_size_env + EXTRA_SIZE_ENV);
  _env[_pos_env++] = _buffer + _pos_buffer;
  _env[_pos_env] = nullptr;
  _pos_buffer += size_name + size_value + 2;
}

/**
 *  Add an environment variable.
 *
 *  @param[in] line  The name and value on form (name=value).
 */
void environment::add(char const* line) {
  if (!line)
    return;
  uint32_t size(strlen(line));
  uint32_t new_pos = _pos_buffer + size + 1;
  if (new_pos > _size_buffer) {
    if (new_pos < _size_buffer + EXTRA_SIZE_BUFFER)
      _realloc_buffer(_size_buffer + EXTRA_SIZE_BUFFER);
    else
      _realloc_buffer(new_pos + EXTRA_SIZE_BUFFER);
  }
  memcpy(_buffer + _pos_buffer, line, size + 1);
  if (_pos_env + 1 >= _size_env)
    _realloc_env(_size_env + EXTRA_SIZE_ENV);
  _env[_pos_env++] = _buffer + _pos_buffer;
  _env[_pos_env] = nullptr;
  _pos_buffer += size + 1;
}
/**
 *  Add an environment variable.
 *
 *  @param[in] line The name and value on form (name=value).
 */

void environment::add(std::string const& line) {
  if (line.empty())
    return;
  uint32_t new_pos(_pos_buffer + line.size() + 1);
  if (new_pos > _size_buffer) {
    if (new_pos < _size_buffer + EXTRA_SIZE_BUFFER)
      _realloc_buffer(_size_buffer + EXTRA_SIZE_BUFFER);
    else
      _realloc_buffer(new_pos + EXTRA_SIZE_BUFFER);
  }
  memcpy(_buffer + _pos_buffer, line.c_str(), line.size() + 1);
  if (_pos_env + 1 >= _size_env)
    _realloc_env(_size_env + EXTRA_SIZE_ENV);
  _env[_pos_env++] = _buffer + _pos_buffer;
  _env[_pos_env] = nullptr;
  _pos_buffer += line.size() + 1;
}

/**
 *  Add an environment variable.
 *
 *  @param[in] name   The name of the environment variable.
 *  @param[in] value  The environment varaible value.
 */

void environment::add(std::string const& name, std::string const& value) {
  if (name.empty())
    return;
  uint32_t new_pos(_pos_buffer + name.size() + value.size() + 2);
  if (new_pos > _size_buffer) {
    if (new_pos < _size_buffer + EXTRA_SIZE_BUFFER)
      _realloc_buffer(_size_buffer + EXTRA_SIZE_BUFFER);
    else
      _realloc_buffer(new_pos + EXTRA_SIZE_BUFFER);
  }
  memcpy(_buffer + _pos_buffer, name.c_str(), name.size() + 1);
  _buffer[_pos_buffer + name.size()] = '=';
  memcpy(_buffer + _pos_buffer + name.size() + 1, value.c_str(),
         value.size() + 1);
  if (_pos_env + 1 >= _size_env)
    _realloc_env(_size_env + EXTRA_SIZE_ENV);
  _env[_pos_env++] = _buffer + _pos_buffer;
  _env[_pos_env] = nullptr;
  _pos_buffer += name.size() + value.size() + 2;
}

/**
 *  Get environment.
 */
char** environment::data() const noexcept {
  return _env;
}

/**
 *  Realoc internal buffer.
 *
 *  @param[in] size  New size.
 */
void environment::_realloc_buffer(uint32_t size) {
  if (_size_buffer >= size)
    throw engine_error()
        << "Invalid size for command environment reallocation: "
        << "Buffer size is greater than the requested size";
  char* new_buffer(new char[size]);
  if (_buffer)
    memcpy(new_buffer, _buffer, _pos_buffer);
  _size_buffer = size;
  delete[] _buffer;
  _buffer = new_buffer;
  _rebuild_env();
}

/**
 *  Realoc internal env array.
 *
 *  @param[in] size  New size.
 */
void environment::_realloc_env(uint32_t size) {
  if (_size_env >= size)
    throw(engine_error()
          << "Invalid size for command environment reallocation: "
          << "Environment size is greater than the requested size");
  char** new_env(new char*[size]);
  if (_env)
    memcpy(new_env, _env, sizeof(*new_env) * (_pos_env + 1));
  _size_env = size;
  delete[] _env;
  _env = new_env;
  return;
}

/**
 *  Rebuild environment array.
 */
void environment::_rebuild_env() {
  if (!_env)
    return;
  int64_t delta = _env[0] - _buffer;
  for (size_t i = 0; i < _pos_env; ++i)
    _env[i] -= delta;
}
