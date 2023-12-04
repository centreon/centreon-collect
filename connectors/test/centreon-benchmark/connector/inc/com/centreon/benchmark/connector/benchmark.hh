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

#ifndef CCB_CONNECTOR_BENCHMARK
#define CCB_CONNECTOR_BENCHMARK

#include <fstream>
#include <string>
#include <vector>

CCB_CONNECTOR_BEGIN()

/**
 *  @class benchmark benchmark.hh
 *"com/centreon/benchmark/connector/benchmark.hh"
 *  @brief Base class to create benchmark.
 *
 *  This class is a base to create benchmark.
 */
class benchmark {
 public:
  benchmark();
  benchmark(benchmark const& right);
  ~benchmark() throw();
  benchmark& operator=(benchmark const& right);
  unsigned int get_limit_running() const throw();
  unsigned int get_memory_usage() const throw();
  std::string const& get_output_file() const throw();
  unsigned int get_total_request() const throw();
  virtual void run() = 0;
  void set_limit_running(unsigned int limit) throw();
  void set_memory_usage(unsigned int size) throw();
  void set_output_file(std::string const& file);
  void set_total_request(unsigned int total) throw();

 protected:
  void _write(std::string const& data);
  void _write(char const* data, unsigned int size);

  unsigned int _limit_running;
  unsigned int _total_request;

 private:
  benchmark& _internal_copy(benchmark const& right);

  char* _buffer;
  unsigned int _memory_usage;
  std::ofstream _output;
  std::string _output_file;
};

CCB_CONNECTOR_END()

#endif  // !CCB_CONNECTOR_BENCHMARK
