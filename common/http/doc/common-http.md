# Centreon Common http library documentation

## Introduction

This library relies on low level http library: boost::beast.
The goal of this library is to provide a higher level than boost::beast.
It provides http(s) client and server implementation.
All this library is asynchronous.
In order to use it, you have to provide a shared_ptr<asio::io_context> yet run outside and a shared_ptr<spdlog::logger>.
All is configured in that bean: http_config.
beast::request and beast::response are body templated. We use the boost::beast::http::string_body version
```c++
using request_type = boost::beast::http::request<boost::beast::http::string_body>;
using response_type = boost::beast::http::response<boost::beast::http::string_body>;
```


### Common client/server classes

#### http_config: a bean that contains all configuration options needed by server and client
Fields passed to constructor:
- endpoint: this endpoint is the connection endpoint in case of client or listen endpoint in case of server
- server_name: (not used by this library), but provided to construct http request
  ```c++
  auto ret = std::make_shared<request>(
      boost::beast::http::verb::post, _conf->get_server_name(),
      "action");
  ```
- connect_timeout: timeout to connect to server (tcp connection time, not handshake)
- send_timeout: timeout to send request (client) or response (server)
- receive_timeout: 
  - client: timeout to receive response after have sent request
  - server: max time to wait a request after connection success or after last response sent. This parameter is also added to response via the keep-alive header. If this parameter is lower than 1s, no keep-alive is added to the response and connection is closed after response sent
- second_tcp_keep_alive_interval: if not null, tcp keep alive is activated on each connection in order to maintain NATs
- max_retry_interval: when client fails to send request, client retries to send request, delay between two attempts is doubled until it reaches max_retry_interval  
- max_send_retry: max client send attempts
- default_http_keepalive_duration: When server answers with keep-alive header, but not delay or when where are in http 1.1, this parameter is used by client to disconnect from the server after this delay
- ssl_method: parameter used to initialize ssl context
  - Values allowed: 
    - tlsv1_client
    - tlsv1_server
    - sslv23
    - sslv23_client
    - sslv23_server
    - tlsv11
    - tlsv11_client
    - tlsv11_server
    - tlsv12
    - tlsv12_client
    - tlsv12_server
    - tlsv13
    - tlsv13_client
    - tlsv13_server
    - tls
    - tls_client
    - tls_server
- certificate_path: path of the file that contains certificate (both client and server)
- key_path: path of the file thant contains key of the certificate (server only)

#### request_base
It inherits from boost::beast::http::request< boost::beast::http::string_body > and embed some time-points to get statistics


#### connection_base
This abstract class is the mother of http_connection and https_connection.
It contains an io_context, logger and the peer endpoint
In order to avoid mistakes, it also contains an atomic _state indicator.

- abstract methods:
    - shutdown: shutdown connection, connection won't be reusable after this call. This method returns immediately but is asynchronous in case of https case. So connection object will be deleted later once ssl shutdown have been done.
    - server usage
      - on_accept(): first, the server creates an non connected connection, when it's accepted, it calls on_accept and then this connection can live. The implemented method must call on_accept(connect_callback_type&& callback)
      - on_accept(connect_callback_type&& callback): do the ssl handshake and initialize tcp keep-alive. callback must call receive_request to wait for request
      - receive_request(request_callback_type&& callback): wait for incoming request, it must call answer to send response
      - answer(const response_ptr& response, answer_callback_type&& callback): send answer to client, if a receive_timeout is > one second, implementation must call receive_request to read nex requests, otherwise, socket is closed adn nothing have to be done
      - get_socket: returns a reference to the transport layer (asio::ip::tcp::socket)
    - client usage
      - connect(connect_callback_type&& callback): It connects to the server and performs ssl handshake. callback can then call send
      - send(request_ptr request, send_callback_type&& callback): send request to the server and pass response to callback. On response, connection is closed if server indicates that there is no keep-alive

- implemented methods:
  - gest_keepalive: (client side) It extracts keep-alive header from response and calculate time-point limit after that connection will be shutdown (returned by get_keep_alive_end()).
  - add_keep_alive_to_server_response: (server side) It adds keep-alive header to answer. It uses receive_time out config parameter.

#### http_connection
- init_keep_alive(): activate tcp keep-alive
- load: this object must be created in an shared_ptr (constructor is protected). This method creates it in a shared_ptr<http_connection>

#### https_connection
- init_keep_alive(): activate tcp keep-alive
- load: this object must be created in an shared_ptr (constructor is protected). This method creates it in a shared_ptr<http_connection>

#### http_client
This client uses http_connection or https_connection objects to send request to ONE server.<br/>
Connections are stored in three containers:
- _not_connected_conns: not connected connections
- _keep_alive_conns: connected connections maintained by both client and server after have received a response with http keep-alive activated. These connections are available for next requests.
- _busy_conns: connections active
##### strategy
When we want to send a request, we first try to reuse a connection of _keep_alive_conns.<br/>
If not available, we connect one of _not_connected_conns.<br/>
If not available, request is pushed to queue and will be send as soon as a connection will ba available.
##### retry strategy
In case of failure (transport failure not http error code), request sent is deferred with an asio timer and then repushed in queue. Delay between two attempts is doubled on each send failure.

#### http_server
The job of this class is to only accept incoming connections. Once a connection is accepted, this connection is not owned by http_server object and continue to leave even if http_server has been shutting down.
In order to use it with both http and https, the most simple is to define a session templated class that can inherit either from http_connection or https_connection.
Then you have to implement functors in charge of create of session.
http_server use this functor to create a not connected session. Once it's accepted, it call on_accept() method of the session object, release it and create another session object.

Example of implementation:
- session implementation
```c++
template <class connection_class>
class session_test : public connection_class {

  void wait_for_request();

  void answer_to_request(const std::shared_ptr<request_type>& request);

 public:
  session_test(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const http_config::pointer& conf,
               const ssl_ctx_initializer& ssl_initializer)
      : connection_class(io_context, logger, conf, ssl_initializer) {
  }

  std::shared_ptr<session_test> shared_from_this() {
    return std::static_pointer_cast<session_test>(
        connection_class::shared_from_this());
  }

  void on_accept() override;
};

/**
 * @brief one tcp connection is accepted, start handshake
 * 
 * @tparam connection_class 
 */
template <class connection_class>
void session_test<connection_class>::on_accept() {
// lambda will be called once handshake is done
  connection_class::_on_accept(
      [me = shared_from_this()](const boost::beast::error_code& err,
                                const std::string&) {
        if (!err)
          me->wait_for_request();
      });
}

/**
 * @brief wait incoming request
 * 
 * @tparam connection_class 
 */
template <class connection_class>
void session_test<connection_class>::wait_for_request() {
  connection_class::receive_request(
      [me = shared_from_this()](const boost::beast::error_code& err,
                                const std::string& detail,
                                const std::shared_ptr<request_type>& request) {
        if (err) {
          SPDLOG_LOGGER_DEBUG(me->_logger,
                              "fail to receive request from {}: {}", me->_peer,
                              err.what());
          return;
        }
        me->answer_to_request(request);
      });
}

/**
 * @brief called once a request have been received
 * Once the response has been sent, it calls wait_for_request 
 * to manage next incoming request
 * 
 * @tparam connection_class 
 * @param request 
 */
template <class connection_class>
void session_test<connection_class>::answer_to_request(
    const std::shared_ptr<request_type>& request) {
  response_ptr resp(std::make_shared<response_type>());
  resp->version(request->version());
  resp->body() = request->body();
  resp->content_length(resp->body().length());

  connection_class::answer(
      resp, [me = shared_from_this(), resp](const boost::beast::error_code& err,
                                            const std::string& detail) {
        if (err) {
          SPDLOG_LOGGER_ERROR(me->_logger, "fail to answer to client {} {}",
                              err.message(), detail);
          return;
        }
        me->wait_for_request();
      });
}
```
- server creation
```c++
void create_server(const std::shared_ptr<asio::io_context> & io_ctx, 
            const std::shared_ptr<spdlog::logger &> logger, 
            const http_config::pointer conf) {
    connection_creator server_creator;
    if (conf->is_crypted()) {
        server_creator = [io_ctx, logger, conf]() {
        return std::make_shared<session_test<https_connection>>(
            io_ctx, logger, conf, https_connection::load_server_certificate);
        };
    }
    else {
        server_creator = [io_ctx, logger, conf]() {
        return std::make_shared<session_test<http_connection>>(
            io_ctx, logger, conf);
        };
    }
    auto server = http_server::load(io_ctx, logger, conf,
                             std::move(server_creator));
}
```
