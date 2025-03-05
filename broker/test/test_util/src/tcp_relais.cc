/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "tcp_relais.hh"

using namespace asio::ip;

extern std::shared_ptr<asio::io_context> g_io_context;

namespace com::centreon::broker::test_util::detail {

class incomming_outgoing;

class relay_cont {
  using relay_set = std::set<std::shared_ptr<incomming_outgoing>>;
  relay_set _relays;
  mutable std::recursive_mutex _protect;
  using unique_lock = std::unique_lock<std::recursive_mutex>;

 public:
  void add(const std::shared_ptr<incomming_outgoing>& to_add) {
    unique_lock l(_protect);
    _relays.insert(to_add);
  }

  void remove(const std::shared_ptr<incomming_outgoing>& to_add) {
    unique_lock l(_protect);
    _relays.erase(to_add);
  }

  template <class modifier>
  void apply(modifier&& mod) {
    unique_lock l(_protect);
    for (auto& toapply : _relays) {
      mod(toapply);
    }
  }
};

using relay_set_pointer = std::shared_ptr<relay_cont>;

/**
 * @brief This class is in charge of relaying between incomming and outgoing
 * connexion
 *
 */
class incomming_outgoing
    : public std::enable_shared_from_this<incomming_outgoing> {
  tcp::socket _incomming, _outgoing;
  tcp::endpoint _dest, _from;
  relay_set_pointer _owner;

  static constexpr size_t buff_size = 65536;
  using buff_array = std::array<unsigned char, buff_size>;
  using shared_array = std::shared_ptr<buff_array>;

  void on_connect(const boost::system::error_code&);

  void start_relaying(tcp::socket& receiver, tcp::socket& sender);

  void on_recv(const boost::system::error_code& err,
               tcp::socket& receiver,
               tcp::socket& sender,
               const shared_array&,
               size_t nb_recv);

  void on_sent(const boost::system::error_code& err, tcp::socket& sock);

  void on_error();

 public:
  using pointer = std::shared_ptr<incomming_outgoing>;
  incomming_outgoing(const tcp::endpoint& dest, const relay_set_pointer& owner)
      : _incomming(*g_io_context),
        _outgoing(*g_io_context),
        _dest(dest),
        _owner(owner) {}

  void connect();
  void shutdown();

  friend class tcp_relais_impl;
};

/**
 * @brief This server class accept incoming connections and start the connection
 * to the dest
 *
 */
class tcp_relais_impl : public std::enable_shared_from_this<tcp_relais_impl> {
  tcp::endpoint _dest;

  std::unique_ptr<tcp::acceptor> _acceptor;
  relay_set_pointer _relays;

  void start_accept();
  void on_accept(const incomming_outgoing::pointer& incomming,
                 const boost::system::error_code&);
  tcp_relais_impl(const std::string& listen_interface,
                  unsigned listen_port,
                  const std::string& dest_host,
                  unsigned dest_port);

 public:
  using pointer = std::shared_ptr<tcp_relais_impl>;

  static pointer create(const std::string& listen_interface,
                        unsigned listen_port,
                        const std::string& dest_host,
                        unsigned dest_port);

  tcp_relais_impl(const tcp_relais_impl&) = delete;
  tcp_relais_impl& operator=(const tcp_relais_impl&) = delete;

  void shutdown_relays();
};

}  // namespace com::centreon::broker::test_util::detail

using namespace com::centreon::broker::test_util;
using namespace com::centreon::broker::test_util::detail;

/**************************************************************************
 * tcp_relais
 **************************************************************************/
tcp_relais::tcp_relais(const std::string& listen_interface,
                       unsigned listen_port,
                       const std::string& dest_host,
                       unsigned dest_port)
    : _impl(detail::tcp_relais_impl::create(listen_interface,
                                            listen_port,
                                            dest_host,
                                            dest_port)) {}

tcp_relais::~tcp_relais() {}

void tcp_relais::shutdown_relays() {
  _impl->shutdown_relays();
}

/**************************************************************************
 * incomming_outgoing
 **************************************************************************/

void incomming_outgoing::connect() {
  _outgoing.async_connect(
      _dest, [me = shared_from_this()](const boost::system::error_code& err) {
        me->on_connect(err);
      });
}

void incomming_outgoing::on_connect(const boost::system::error_code& err) {
  if (err) {
    std::cerr << __FUNCTION__ << " echec connect to " << _dest << " : "
              << err.message() << std::endl;
    return;
  }
  start_relaying(_incomming, _outgoing);
  start_relaying(_outgoing, _incomming);
}

void incomming_outgoing::start_relaying(tcp::socket& receiver,
                                        tcp::socket& sender) {
  shared_array recvBuff(std::make_shared<buff_array>());
  receiver.async_receive(
      asio::buffer(*recvBuff), [me = shared_from_this(), recvBuff, &receiver,
                                &sender](const boost::system::error_code& error,
                                         std::size_t bytes_transferred) {
        me->on_recv(error, receiver, sender, recvBuff, bytes_transferred);
      });
}

void incomming_outgoing::on_recv(const boost::system::error_code& err,
                                 tcp::socket& receiver,
                                 tcp::socket& sender,
                                 const shared_array& buff,
                                 size_t nb_recv) {
  if (err) {
    on_error();
    return;
  }

  asio::async_write(sender, asio::buffer(*buff, nb_recv),
                    [me = shared_from_this(), buff, &sender](
                        const boost::system::error_code& err, size_t) {
                      me->on_sent(err, sender);
                    });
  start_relaying(receiver, sender);
}

void incomming_outgoing::on_sent(const boost::system::error_code& err,
                                 tcp::socket& sock [[maybe_unused]]) {
  if (err) {
    on_error();
    return;
  }
}

void incomming_outgoing::on_error() {
  shutdown();
  _owner->remove(shared_from_this());
}

void incomming_outgoing::shutdown() {
  try {
    _outgoing.shutdown(tcp::socket::shutdown_both);
  } catch (const std::exception&) {
  }
  try {
    _incomming.shutdown(tcp::socket::shutdown_both);
  } catch (const std::exception&) {
  }
}
/**************************************************************************
 * tcp_relais_impl
 **************************************************************************/

tcp_relais_impl::tcp_relais_impl(const std::string& listen_interface,
                                 unsigned listen_port,
                                 const std::string& dest_host,
                                 unsigned dest_port)
    : _relays(std::make_shared<relay_cont>()) {
  tcp::resolver r(*g_io_context);
  tcp::resolver::results_type endpoints =
      r.resolve(listen_interface, std::to_string(listen_port));
  if (endpoints.empty()) {
    std::cerr << __FUNCTION__ << "unable to resolve " << listen_interface << ':'
              << listen_port << std::endl;
    throw(std::invalid_argument("unable to resolve " + listen_interface + ':' +
                                std::to_string(listen_port)));
  }

  tcp::resolver::results_type dest_endpoints =
      r.resolve(dest_host, std::to_string(dest_port));
  if (dest_endpoints.empty()) {
    std::cerr << __FUNCTION__ << "unable to resolve " << dest_host << ':'
              << dest_host << std::endl;
    throw(std::invalid_argument("unable to resolve " + dest_host + ':' +
                                std::to_string(dest_port)));
  }
  _dest = *dest_endpoints.begin();
  _acceptor =
      std::make_unique<tcp::acceptor>(*g_io_context, *endpoints.begin());
}

tcp_relais_impl::pointer tcp_relais_impl::create(
    const std::string& listen_interface,
    unsigned listen_port,
    const std::string& dest_host,
    unsigned dest_port) {
  pointer ret(
      new tcp_relais_impl(listen_interface, listen_port, dest_host, dest_port));
  ret->start_accept();
  return ret;
}

void tcp_relais_impl::start_accept() {
  incomming_outgoing::pointer to_accept(
      std::make_shared<incomming_outgoing>(_dest, _relays));
  _acceptor->async_accept(to_accept->_incomming, to_accept->_from,
                          [me = shared_from_this(),
                           to_accept](const boost::system::error_code& err) {
                            me->on_accept(to_accept, err);
                          });
}

void tcp_relais_impl::on_accept(const incomming_outgoing::pointer& incomming,
                                const boost::system::error_code& err) {
  if (err) {
    std::cerr << __FUNCTION__ << " echec accept " << _acceptor->local_endpoint()
              << " : " << err.message() << std::endl;
    return;
  }
  _relays->add(incomming);
  incomming->connect();
  start_accept();
}

void tcp_relais_impl::shutdown_relays() {
  _relays->apply([](const incomming_outgoing::pointer& to_shutdown) {
    to_shutdown->shutdown();
  });
}
