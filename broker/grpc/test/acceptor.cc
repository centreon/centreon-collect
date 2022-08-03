/*
 * Copyright 2021 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/tcp/acceptor.hh"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "com/centreon/broker/misc/buffer.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/grpc/connector.hh"
#include "grpc_test_include.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;
using namespace com::centreon::exceptions;

class GrpcTlsTest : public ::testing::Test {
 public:
  void SetUp() override {
    pool::load(0);
  }

  void TearDown() override {
    pool::unload();
  }
};

static auto read_file = [] (const std::string& path) {
  std::ifstream file(path);
  std::stringstream ss;
  ss << file.rdbuf();
  file.close();
  return ss.str();
};

TEST_F(GrpcTlsTest, TlsStream) {
  /* Let's prepare certificates */
  std::string hostname = misc::exec("hostname --fqdn");
  hostname = misc::string::trim(hostname);
  std::string server_cmd(
      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
                  "-keyout /tmp/server.key -out /tmp/server.crt -subj '/CN={}'",
                  hostname));
  std::cout << server_cmd << std::endl;
  system(server_cmd.c_str());

  std::string client_cmd(
      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
                  "-keyout /tmp/client.key -out /tmp/client.crt -subj '/CN={}'",
                  hostname));
  std::cout << client_cmd << std::endl;
  system(client_cmd.c_str());

  std::atomic_bool cbd_finished{false};

  std::thread cbd([&cbd_finished] {
    auto conf{std::make_shared<grpc_config>("0.0.0.0:4141",
        true,
        read_file("/tmp/server.crt"),
        read_file("/tmp/server.key"),
        read_file("/tmp/client.crt"),
        "",
        "centreon",
        grpc_config::NO)};
    auto a{std::make_unique<acceptor>(conf)};

    /* Nominal case, cbd is acceptor and read on the socket */
    std::shared_ptr<io::stream> tls_cbd;
    do {
      tls_cbd = a->open();
    } while (!tls_cbd);

    do {
      std::shared_ptr<io::data> d;
      bool no_timeout = tls_cbd->read(d, 0);
      if (no_timeout) {
        io::raw* rr = static_cast<io::raw*>(d.get());
        ASSERT_EQ(strncmp(rr->data(), "Hello cbd", 0), 0);
        break;
      }
      std::this_thread::yield();
    } while (true);

    cbd_finished = true;
  });

  std::thread centengine([&cbd_finished] {
    auto conf{std::make_shared<grpc_config>("localhost:4141",
        true,
        read_file("/tmp/client.crt"),
        read_file("/tmp/client.key"),
        read_file("/tmp/server.crt"),
        "",
        "",
        grpc_config::NO)};
    auto c{std::make_unique<connector>(conf)};

    /* Nominal case, centengine is connector and write on the socket */
    std::shared_ptr<io::stream> tls_centengine;
    do {
      tls_centengine = c->open();
    } while (!tls_centengine);

    std::vector<char> v{'H', 'e', 'l', 'l', 'o', ' ', 'c', 'b', 'd'};
    auto packet = std::make_shared<io::raw>(std::move(v));

    /* This is not representative of a real stream. Here we have to call write
     * several times, so that the SSL library makes its work in the back */
    do {
      tls_centengine->write(packet);
      std::this_thread::yield();
    } while (!cbd_finished);
  });

  centengine.join();
  cbd.join();
}

TEST_F(GrpcTlsTest, TlsStreamCaHostname) {
  /* Let's prepare certificates */
  const static std::string s_hostname{"saperlifragilistic"};
  const static std::string c_hostname{"foobar"};
  std::string server_cmd(
      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
                  "-keyout /tmp/server.key -out /tmp/server.crt -subj '/CN={}'",
                  s_hostname));
  std::cout << server_cmd << std::endl;
  system(server_cmd.c_str());

  std::string client_cmd(
      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
                  "-keyout /tmp/client.key -out /tmp/client.crt -subj '/CN={}'",
                  c_hostname));
  std::cout << client_cmd << std::endl;
  system(client_cmd.c_str());

  std::atomic_bool cbd_finished{false};

  std::thread cbd([&cbd_finished] {
    auto conf{std::make_shared<grpc_config>("0.0.0.0:4141",
        true,
        read_file("/tmp/server.crt"),
        read_file("/tmp/server.key"),
        read_file("/tmp/client.crt"),
        "",
        "centreon",
        grpc_config::NO)};
    auto a{std::make_unique<acceptor>(conf)};

    /* Nominal case, cbd is acceptor and read on the socket */
    std::shared_ptr<io::stream> tls_cbd;
    do {
      tls_cbd = a->open();
    } while (!tls_cbd);

    do {
      std::shared_ptr<io::data> d;
      bool no_timeout = tls_cbd->read(d, 0);
      if (no_timeout) {
        io::raw* rr = static_cast<io::raw*>(d.get());
        ASSERT_EQ(strncmp(rr->data(), "Hello cbd", 0), 0);
        break;
      }
      std::this_thread::yield();
    } while (true);

    cbd_finished = true;
  });

  std::thread centengine([&cbd_finished] {
    auto conf{std::make_shared<grpc_config>("localhost:4141",
        true,
        read_file("/tmp/client.crt"),
        read_file("/tmp/client.key"),
        read_file("/tmp/server.crt"),
        "",
        s_hostname,
        grpc_config::NO)};
    auto c{std::make_unique<connector>(conf)};

    /* Nominal case, centengine is connector and write on the socket */
    std::shared_ptr<io::stream> tls_centengine;
    do {
      tls_centengine = c->open();
    } while (!tls_centengine);

    std::vector<char> v{'H', 'e', 'l', 'l', 'o', ' ', 'c', 'b', 'd'};
    auto packet = std::make_shared<io::raw>(std::move(v));

    /* This is not representative of a real stream. Here we have to call write
     * several times, so that the SSL library makes its work in the back */
    do {
      tls_centengine->write(packet);
      std::this_thread::yield();
    } while (!cbd_finished);
  });

  centengine.join();
  cbd.join();
}
//TEST_F(GrpcTlsTest, TlsStreamCaHostname) {
//  /* Let's prepare certificates */
//  const static std::string s_hostname{"saperlifragilistic"};
//  const static std::string c_hostname{"foobar"};
//  std::string server_cmd(
//      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
//                  "-keyout /tmp/server.key -out /tmp/server.crt -subj '/CN={}'",
//                  s_hostname));
//  std::cout << server_cmd << std::endl;
//  system(server_cmd.c_str());
//
//  std::string client_cmd(
//      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
//                  "-keyout /tmp/client.key -out /tmp/client.crt -subj '/CN={}'",
//                  c_hostname));
//  std::cout << client_cmd << std::endl;
//  system(client_cmd.c_str());
//
//  std::atomic_bool cbd_finished{false};
//
//  std::thread cbd([&cbd_finished] {
//    auto a{std::make_unique<tcp::acceptor>("", 4141, -1)};
//
//    auto tls_a{std::make_unique<tls::acceptor>(
//        "/tmp/server.crt", "/tmp/server.key", "/tmp/client.crt", "")};
//
//    /* Nominal case, cbd is acceptor and read on the socket */
//    std::shared_ptr<io::stream> u_cbd;
//    do {
//      u_cbd = a->open();
//    } while (!u_cbd);
//
//    std::unique_ptr<io::stream> io_tls_cbd = tls_a->open(u_cbd);
//    tls::stream* tls_cbd = static_cast<tls::stream*>(io_tls_cbd.get());
//
//    do {
//      std::shared_ptr<io::data> d;
//      puts("cbd read");
//      bool no_timeout = tls_cbd->read(d, 0);
//      if (no_timeout) {
//        io::raw* rr = static_cast<io::raw*>(d.get());
//        ASSERT_EQ(strncmp(rr->data(), "Hello cbd", 0), 0);
//        break;
//      }
//      std::this_thread::yield();
//    } while (true);
//
//    cbd_finished = true;
//  });
//
//  std::thread centengine([&cbd_finished] {
//    auto c{std::make_unique<tcp::connector>("localhost", 4141, -1)};
//
//    auto tls_c{std::make_unique<tls::connector>(
//        "/tmp/client.crt", "/tmp/client.key", "/tmp/server.crt", s_hostname)};
//
//    /* Nominal case, centengine is connector and write on the socket */
//    std::shared_ptr<io::stream> u_centengine;
//    do {
//      u_centengine = c->open();
//    } while (!u_centengine);
//
//    std::unique_ptr<io::stream> io_tls_centengine{tls_c->open(u_centengine)};
//    tls::stream* tls_centengine =
//        static_cast<tls::stream*>(io_tls_centengine.get());
//
//    std::vector<char> v{'H', 'e', 'l', 'l', 'o', ' ', 'c', 'b', 'd'};
//    auto packet = std::make_shared<io::raw>(std::move(v));
//
//    /* This is not representative of a real stream. Here we have to call write
//     * several times, so that the SSL library makes its work in the back */
//    do {
//      puts("centengine write");
//      tls_centengine->write(packet);
//      std::this_thread::yield();
//    } while (!cbd_finished);
//  });
//
//  centengine.join();
//  cbd.join();
//}
//
//TEST_F(GrpcTlsTest, TlsStreamBigData) {
//  using namespace std::chrono_literals;
//
//  /* Let's prepare certificates */
//  const static std::string s_hostname{"saperlifragilistic"};
//  const static std::string c_hostname{"foobar"};
//  const static int max_limit = 20;
//  std::string server_cmd(
//      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
//                  "-keyout /tmp/server.key -out /tmp/server.crt -subj '/CN={}'",
//                  s_hostname));
//  std::cout << server_cmd << std::endl;
//  system(server_cmd.c_str());
//
//  std::string client_cmd(
//      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
//                  "-keyout /tmp/client.key -out /tmp/client.crt -subj '/CN={}'",
//                  c_hostname));
//  std::cout << client_cmd << std::endl;
//  system(client_cmd.c_str());
//
//  std::atomic_bool cbd_finished{false};
//
//  std::thread cbd([&cbd_finished] {
//    auto a{std::make_unique<tcp::acceptor>("", 4141, -1)};
//
//    auto tls_a{std::make_unique<tls::acceptor>(
//        "/tmp/server.crt", "/tmp/server.key", "/tmp/client.crt", "")};
//
//    /* Nominal case, cbd is acceptor and read on the socket */
//    std::shared_ptr<io::stream> u_cbd;
//    do {
//      u_cbd = a->open();
//    } while (!u_cbd);
//
//    std::unique_ptr<io::stream> io_tls_cbd = tls_a->open(u_cbd);
//    tls::stream* tls_cbd = static_cast<tls::stream*>(io_tls_cbd.get());
//
//    char c = 'A';
//    size_t length = 26u;
//    std::vector<char> v(length, c);
//    size_t limit = 1;
//    std::vector<char> my_vector;
//    do {
//      std::shared_ptr<io::data> d;
//      bool no_timeout = tls_cbd->read(d, 0);
//      if (no_timeout) {
//        io::raw* rr = static_cast<io::raw*>(d.get());
//        my_vector.insert(my_vector.end(), rr->get_buffer().begin(),
//                         rr->get_buffer().end());
//      }
//      if (!my_vector.empty() && my_vector.size() >= v.size() &&
//          memcmp(my_vector.data(), v.data(), v.size()) == 0) {
//        my_vector.erase(my_vector.begin(), my_vector.begin() + v.size());
//        limit++;
//        ASSERT_TRUE(true);
//        if (limit > max_limit)
//          break;
//        c++;
//        length *= 2u;
//        v = std::vector<char>(length, c);
//      } else if (my_vector.size() >= v.size()) {
//        ASSERT_TRUE(false);
//        break;
//      }
//
//      std::this_thread::yield();
//    } while (true);
//
//    cbd_finished = true;
//  });
//
//  std::thread centengine([&cbd_finished] {
//    auto c{std::make_unique<tcp::connector>("localhost", 4141, -1)};
//
//    auto tls_c{std::make_unique<tls::connector>(
//        "/tmp/client.crt", "/tmp/client.key", "/tmp/server.crt", s_hostname)};
//
//    /* Nominal case, centengine is connector and write on the socket */
//    std::shared_ptr<io::stream> u_centengine;
//    do {
//      u_centengine = c->open();
//    } while (!u_centengine);
//
//    std::unique_ptr<io::stream> io_tls_centengine{tls_c->open(u_centengine)};
//    tls::stream* tls_centengine =
//        static_cast<tls::stream*>(io_tls_centengine.get());
//
//    char ch = 'A';
//    size_t length = 26u;
//    std::vector<char> v(length, ch);
//
//    for (size_t limit = 1; limit <= max_limit;) {
//      auto packet = std::make_shared<io::raw>(v);
//
//      tls_centengine->write(packet);
//      ch++;
//      limit++;
//      // std::this_thread::sleep_for(1ms);
//      length *= 2;
//      v = std::vector<char>(length, ch);
//    }
//    do {
//      std::this_thread::yield();
//    } while (!cbd_finished);
//  });
//
//  centengine.join();
//  cbd.join();
//}
//
//TEST_F(GrpcTlsTest, TlsStreamLongData) {
//  using namespace std::chrono_literals;
//
//  /* Let's prepare certificates */
//  const static std::string s_hostname{"saperlifragilistic"};
//  const static std::string c_hostname{"foobar"};
//  const static int max_limit = 20000;
//  std::string server_cmd(
//      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
//                  "-keyout /tmp/server.key -out /tmp/server.crt -subj '/CN={}'",
//                  s_hostname));
//  std::cout << server_cmd << std::endl;
//  system(server_cmd.c_str());
//
//  std::string client_cmd(
//      fmt::format("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 "
//                  "-keyout /tmp/client.key -out /tmp/client.crt -subj '/CN={}'",
//                  c_hostname));
//  std::cout << client_cmd << std::endl;
//  system(client_cmd.c_str());
//
//  std::atomic_bool cbd_finished{false};
//
//  std::thread cbd([&cbd_finished] {
//    auto a{std::make_unique<tcp::acceptor>("", 4141, -1)};
//
//    auto tls_a{std::make_unique<tls::acceptor>(
//        "/tmp/server.crt", "/tmp/server.key", "/tmp/client.crt", "")};
//
//    /* Nominal case, cbd is acceptor and read on the socket */
//    std::shared_ptr<io::stream> u_cbd;
//    do {
//      u_cbd = a->open();
//    } while (!u_cbd);
//
//    std::unique_ptr<io::stream> io_tls_cbd = tls_a->open(u_cbd);
//    tls::stream* tls_cbd = static_cast<tls::stream*>(io_tls_cbd.get());
//
//    char c = 'A';
//    size_t length = 26u;
//    std::vector<char> v(length, c);
//    size_t limit = 1;
//    std::vector<char> my_vector;
//    do {
//      std::shared_ptr<io::data> d;
//      bool no_timeout = tls_cbd->read(d, 0);
//      if (no_timeout) {
//        io::raw* rr = static_cast<io::raw*>(d.get());
//        my_vector.insert(my_vector.end(), rr->get_buffer().begin(),
//                         rr->get_buffer().end());
//      }
//      if (!my_vector.empty() &&
//          memcmp(my_vector.data(), v.data(), v.size()) == 0) {
//        my_vector.erase(my_vector.begin(), my_vector.begin() + v.size());
//        limit++;
//        ASSERT_TRUE(true);
//        if (limit > max_limit)
//          break;
//        c++;
//        if (c > 'z')
//          c = 'A';
//        v = std::vector<char>(length, c);
//      } else if (my_vector.size() >= v.size()) {
//        ASSERT_TRUE(false);
//        break;
//      }
//
//      std::this_thread::yield();
//    } while (true);
//
//    cbd_finished = true;
//  });
//
//  std::thread centengine([&cbd_finished] {
//    auto c{std::make_unique<tcp::connector>("localhost", 4141, -1)};
//
//    auto tls_c{std::make_unique<tls::connector>(
//        "/tmp/client.crt", "/tmp/client.key", "/tmp/server.crt", s_hostname)};
//
//    /* Nominal case, centengine is connector and write on the socket */
//    std::shared_ptr<io::stream> u_centengine;
//    do {
//      u_centengine = c->open();
//    } while (!u_centengine);
//
//    std::unique_ptr<io::stream> io_tls_centengine{tls_c->open(u_centengine)};
//    tls::stream* tls_centengine =
//        static_cast<tls::stream*>(io_tls_centengine.get());
//
//    char ch = 'A';
//    size_t length = 26u;
//    std::vector<char> v(length, ch);
//
//    for (size_t limit = 1; limit <= max_limit;) {
//      auto packet = std::make_shared<io::raw>(v);
//
//      tls_centengine->write(packet);
//      ch++;
//      if (ch > 'z')
//        ch = 'A';
//      limit++;
//      // std::this_thread::sleep_for(1ms);
//      v = std::vector<char>(length, ch);
//    }
//    do {
//      std::this_thread::yield();
//    } while (!cbd_finished);
//  });
//
//  centengine.join();
//  cbd.join();
//}
