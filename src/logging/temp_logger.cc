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

#include "com/centreon/logging/temp_logger.hh"

using namespace com::centreon::logging;

temp_logger::redirector const temp_logger::_redir_nothing = {
  &temp_logger::_nothing<char>,
  &temp_logger::_nothing<double>,
  &temp_logger::_nothing<int>,
  &temp_logger::_nothing,
  &temp_logger::_nothing<long long>,
  &temp_logger::_nothing<long>,
  &temp_logger::_nothing<setprecision const&>,
  &temp_logger::_nothing<std::string const&>,
  &temp_logger::_nothing<char const*>,
  &temp_logger::_nothing<unsigned int>,
  &temp_logger::_nothing<unsigned long long>,
  &temp_logger::_nothing<unsigned long>,
  &temp_logger::_nothing<void const*>
};

temp_logger::redirector const temp_logger::_redir_builder = {
  &temp_logger::_builder<char>,
  &temp_logger::_builder<double>,
  &temp_logger::_builder<int>,
  &temp_logger::_flush,
  &temp_logger::_builder<long long>,
  &temp_logger::_builder<long>,
  &temp_logger::_builder_setprecision,
  &temp_logger::_builder<std::string const&>,
  &temp_logger::_builder<char const*>,
  &temp_logger::_builder<unsigned int>,
  &temp_logger::_builder<unsigned long long>,
  &temp_logger::_builder<unsigned long>,
  &temp_logger::_builder<void const*>
};

/**
 *  Default constrcutor.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 */
temp_logger::temp_logger(
               unsigned int type,
               verbosity verbose) throw ()
  : _engine(engine::instance()),
    _redirector(&_redir_nothing),
    _type(type),
    _verbose(verbose) {
  if (_engine.is_log(type, verbose))
    _redirector = &_redir_builder;
}

/**
 *  Default copy constrcutor.
 *
 *  @param[in] right  The object to copy.
 */
temp_logger::temp_logger(temp_logger const& right)
  : _engine(right._engine) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
temp_logger::~temp_logger() throw () {
  (this->*(_redirector->redir_flush))();
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator=(temp_logger const& right) {
  return (_internal_copy(right));
}

/**
 *  Add char into the logger buffer.
 *
 *  @param[in] obj The char to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(char obj) throw () {
  return ((this->*(_redirector->redir_char))(obj));
}

/**
 *  Add double into the logger buffer.
 *
 *  @param[in] obj The double to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(double obj) throw () {
  return ((this->*(_redirector->redir_double))(obj));
}

/**
 *  Add integer into the logger buffer.
 *
 *  @param[in] obj The integer to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(int obj) throw () {
  return ((this->*(_redirector->redir_int))(obj));
}

/**
 *  Add long long into the logger buffer.
 *
 *  @param[in] obj The long long to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(long long obj) throw () {
  return ((this->*(_redirector->redir_long_long))(obj));
}

/**
 *  Add long into the logger buffer.
 *
 *  @param[in] obj The long to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(long obj) throw () {
  return ((this->*(_redirector->redir_long))(obj));
}

/**
 *  Set float precision.
 *
 *  @param[in] obj The new precision.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(setprecision const& obj) throw () {
  return ((this->*(_redirector->redir_setprecision))(obj));
}

/**
 *  Add a string into the logger buffer.
 *
 *  @param[in] obj The string to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(std::string const& obj) throw () {
  return ((this->*(_redirector->redir_std_string))(obj));
}

/**
 *  Add string into the logger buffer.
 *
 *  @param[in] obj The string to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(char const* obj) throw () {
  return ((this->*(_redirector->redir_string))(obj));
}

/**
 *  Add unsigned integer into the logger buffer.
 *
 *  @param[in] obj The unsigned integer to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(unsigned int obj) throw () {
  return ((this->*(_redirector->redir_uint))(obj));
}

/**
 *  Add unsigned long long into the logger buffer.
 *
 *  @param[in] obj The unsigned long long to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(unsigned long long obj) throw () {
  return ((this->*(_redirector->redir_ulong_long))(obj));
}

/**
 *  Add unsigned long into the logger buffer.
 *
 *  @param[in] obj The unsigned long to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(unsigned long obj) throw () {
  return ((this->*(_redirector->redir_ulong))(obj));
}

/**
 *  Add a pointer address into the logger buffer.
 *
 *  @param[in] obj The pointer address to add.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(void const* obj) throw () {
  return ((this->*(_redirector->redir_void_ptr))(obj));
}

/**
 *  Wrapper to used builder redirector.
 *
 *  @param[in] obj  The object to add into the logger buffer.
 *
 *  @return This object.
 */
template<typename T>
temp_logger& temp_logger::_builder(T obj) throw () {
  _buffer << obj;
  return (*this);
}

/**
 *  Set float precision.
 *
 *  @param[in] obj  Set precision.
 *
 *  @return This object.
 */
temp_logger& temp_logger::_builder_setprecision(
                            setprecision const& obj) throw () {
  _buffer.precision(obj.precision);
  return (*this);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
temp_logger& temp_logger::_internal_copy(temp_logger const& right) {
  if (this != &right) {
    _buffer = right._buffer;
    _redirector = right._redirector;
    _type = right._type;
    _verbose = right._verbose;
    right._redirector = &_redir_nothing;
  }
  return (*this);
}

/**
 *  Forces write buffered data.
 *
 *  @return This object.
 */
temp_logger& temp_logger::_flush() throw (){
  _engine.log(_type, _verbose, _buffer.data());
  return (*this);
}

/**
 *  Do noting.
 *
 *  @param[in] obj  Unused parameter.
 *
 *  @return This object.
 */
template<typename T>
temp_logger& temp_logger::_nothing(T obj) throw () {
  (void)obj;
  return (*this);
}

/**
 *  Do noting.
 *
 *  @return This object.
 */
temp_logger& temp_logger::_nothing() throw () {
  return (*this);
}
