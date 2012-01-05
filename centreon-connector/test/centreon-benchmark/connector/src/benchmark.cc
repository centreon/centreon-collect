/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Benchmark ICMP.
**
** Centreon Benchmark ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Benchmark ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Benchmark ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include "com/centreon/benchmark/connector/basic_exception.hh"
#include "com/centreon/benchmark/connector/benchmark.hh"

using namespace com::centreon::benchmark::connector;

/**
 *  Default constructor.
 */
benchmark::benchmark()
  : _limit_running(1),
    _total_request(1024),
    _buffer(NULL),
    _memory_usage(0) {


}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
benchmark::benchmark(benchmark const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
benchmark::~benchmark() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
benchmark& benchmark::operator=(benchmark const& right) {
  return (_internal_copy(right));
}

/**
 *  Get the number of check execute simultaneously.
 *
 *  @return The limit of simultaneous request.
 */
unsigned int benchmark::get_limit_running() const throw() {
  return (_limit_running);
}

/**
 *  Get the memory usage.
 *
 *  @return Size of memory usage.
 */
unsigned int benchmark::get_memory_usage() const throw () {
  return (_memory_usage / 1024 / 1024);
}

/**
 *  Get output file.
 *
 *  @return The file path.
 */
std::string const& benchmark::get_output_file() const throw() {
  return (_output_file);
}

/**
 *  Get the number of request execute by the benchmark.
 *
 *  @return The total request.
 */
unsigned int benchmark::get_total_request() const throw() {
  return (_total_request);
}

/**
 *  Set the number of check execute simultaneously.
 *
 *  @param[in] limit  The limit of simultaneous request.
 */
void benchmark::set_limit_running(unsigned int limit) throw () {
  _limit_running = limit;
}

/**
 *  Set and alocate memory.
 *
 *  @param[in] size  The memory size.
 */
void benchmark::set_memory_usage(unsigned int size) throw () {
  _memory_usage = size * 1024 * 1024;
  delete[] _buffer;
  if (_memory_usage) {
    _buffer = new char[_memory_usage];
    memset(_buffer, 0, _memory_usage);
  }
  else
    _buffer = NULL;
}

/**
 *  Set output file.
 *
 *  @param[in] file  The file path.
 */
void benchmark::set_output_file(std::string const& file) {
  _output_file = file;
  _output.open(
            _output_file.c_str(),
            std::ofstream::binary | std::ofstream::trunc);
  if (!_output.is_open())
    throw (basic_exception("failed to open output file"));
}

/**
 *  Set the number of request execute by the benchmark.
 *
 *  @param[in] total  The total request.
 */
void benchmark::set_total_request(unsigned int total) throw () {
  _total_request = total;
}

/**
 *  If output file is open, write data.
 *
 *  @param[in] data  The data to write.
 */
void benchmark::_write(std::string const& data) {
  _write(data.c_str(), data.size());
}

/**
 *  If output file is open, write data.
 *
 *  @param[in] data  The data to write.
 *  @param[in] size  The data size.
 */
void benchmark::_write(char const* data, unsigned int size) {
  if (_output.is_open())
    _output.write(data, size);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
benchmark& benchmark::_internal_copy(benchmark const& right) {
  if (this != &right) {
    _limit_running = right._limit_running;
    set_memory_usage(right.get_memory_usage());
    set_output_file(right._output_file);
    _total_request = right._total_request;
  }
  return (*this);
}
