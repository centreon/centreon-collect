/*
** Copyright 2022 Centreon
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

#ifndef CCC_ORDERS_PARSER_HH
#define CCC_ORDERS_PARSER_HH

#include "com/centreon/connector/namespace.hh"

CCC_BEGIN()

class policy_interface;

constexpr unsigned parser_buff_size = 4096;

/**
 *  @class parser parser.hh "com/centreon/connector/ssh/orders/parser.hh"
 *  @brief Parse orders.
 *
 *  Parse orders, generally issued by the monitoring engine. The
 *  parser class can handle be registered with one handle at a time
 *  and one listener.
 */
class parser : public std::enable_shared_from_this<parser> {
 protected:
  shared_io_context _io_context;
  asio::posix::stream_descriptor _sin;
  std::string _buffer;
  bool _dont_care_about_stdin_eof;

  std::shared_ptr<policy_interface> _owner;

  char _recv_buff[parser_buff_size];

  void read();
  void _parse(std::string const& cmd);

  parser(const shared_io_context& io_context,
         const std::shared_ptr<policy_interface>& policy);

  virtual void start_read();
  void read_file(const std::string& test_file_path);

  void read_handler(const boost::system::error_code& error,
                    std::size_t bytes_transferred);

  virtual void execute(const std::string& cmd) = 0;

 public:
  using pointer = std::shared_ptr<parser>;

  static const boost::system::error_code eof_err;  // used by test

  virtual ~parser() = default;

  parser(parser const& p) = delete;
  parser& operator=(parser const& p) = delete;
};

CCC_END()

#endif  // !CCC_ORDERS_PARSER_HH
