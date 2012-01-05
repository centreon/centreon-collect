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

#ifndef CCB_CONNECTOR_BENCHMARK
#  define CCB_CONNECTOR_BENCHMARK

#  include <fstream>
#  include <string>
#  include <vector>
#  include "com/centreon/benchmark/connector/namespace.hh"

CCB_CONNECTOR_BEGIN()

/**
 *  @class benchmark benchmark.hh "com/centreon/benchmark/connector/benchmark.hh"
 *  @brief Base class to create benchmark.
 *
 *  This class is a base to create benchmark.
 */
class                        benchmark {
public:
                             benchmark();
                             benchmark(benchmark const& right);
                             ~benchmark() throw ();
  benchmark&                 operator=(benchmark const& right);
  unsigned int               get_limit_running() const throw();
  unsigned int               get_memory_usage() const throw ();
  std::string const&         get_output_file() const throw ();
  unsigned int               get_total_request() const throw();
  virtual void               run() = 0;
  void                       set_limit_running(unsigned int limit) throw ();
  void                       set_memory_usage(unsigned int size) throw ();
  void                       set_output_file(std::string const& file);
  void                       set_total_request(unsigned int total) throw ();

protected:
  void                      _write(std::string const& data);
  void                      _write(char const* data, unsigned int size);

  unsigned int               _limit_running;
  unsigned int               _total_request;

private:
  benchmark&                 _internal_copy(benchmark const& right);

  char*                      _buffer;
  unsigned int               _memory_usage;
  std::ofstream              _output;
  std::string                _output_file;
};

CCB_CONNECTOR_END()

#endif // !CCB_CONNECTOR_BENCHMARK
