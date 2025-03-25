/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#include <gtest/gtest.h>

#include <spdlog/sinks/stdout_color_sinks.h>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#include "http_connection.hh"
#include "http_server.hh"
#include "https_connection.hh"

using namespace com::centreon::common::http;
namespace beast = boost::beast;

extern std::shared_ptr<asio::io_context> g_io_context;

constexpr unsigned port = 5796;

const asio::ip::tcp::endpoint test_endpoint(asio::ip::make_address("127.0.0.1"),
                                            port);

const asio::ip::tcp::endpoint listen_endpoint(asio::ip::make_address("0.0.0.0"),
                                              port);

static void create_client_certificate(const std::string& path) {
  ::remove(path.c_str());
  std::ofstream f(path);
  f << "# ACCVRAIZ1\n"
       "-----BEGIN CERTIFICATE-----\n"
       "MIIH0zCCBbugAwIBAgIIXsO3pkN/pOAwDQYJKoZIhvcNAQEFBQAwQjESMBAGA1UE\n"
       "AwwJQUNDVlJBSVoxMRAwDgYDVQQLDAdQS0lBQ0NWMQ0wCwYDVQQKDARBQ0NWMQsw\n"
       "CQYDVQQGEwJFUzAeFw0xMTA1MDUwOTM3MzdaFw0zMDEyMzEwOTM3MzdaMEIxEjAQ\n"
       "BgNVBAMMCUFDQ1ZSQUlaMTEQMA4GA1UECwwHUEtJQUNDVjENMAsGA1UECgwEQUND\n"
       "VjELMAkGA1UEBhMCRVMwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCb\n"
       "qau/YUqXry+XZpp0X9DZlv3P4uRm7x8fRzPCRKPfmt4ftVTdFXxpNRFvu8gMjmoY\n"
       "HtiP2Ra8EEg2XPBjs5BaXCQ316PWywlxufEBcoSwfdtNgM3802/J+Nq2DoLSRYWo\n"
       "G2ioPej0RGy9ocLLA76MPhMAhN9KSMDjIgro6TenGEyxCQ0jVn8ETdkXhBilyNpA\n"
       "lHPrzg5XPAOBOp0KoVdDaaxXbXmQeOW1tDvYvEyNKKGno6e6Ak4l0Squ7a4DIrhr\n"
       "IA8wKFSVf+DuzgpmndFALW4ir50awQUZ0m/A8p/4e7MCQvtQqR0tkw8jq8bBD5L/\n"
       "0KIV9VMJcRz/RROE5iZe+OCIHAr8Fraocwa48GOEAqDGWuzndN9wrqODJerWx5eH\n"
       "k6fGioozl2A3ED6XPm4pFdahD9GILBKfb6qkxkLrQaLjlUPTAYVtjrs78yM2x/47\n"
       "4KElB0iryYl0/wiPgL/AlmXz7uxLaL2diMMxs0Dx6M/2OLuc5NF/1OVYm3z61PMO\n"
       "m3WR5LpSLhl+0fXNWhn8ugb2+1KoS5kE3fj5tItQo05iifCHJPqDQsGH+tUtKSpa\n"
       "cXpkatcnYGMN285J9Y0fkIkyF/hzQ7jSWpOGYdbhdQrqeWZ2iE9x6wQl1gpaepPl\n"
       "uUsXQA+xtrn13k/c4LOsOxFwYIRKQ26ZIMApcQrAZQIDAQABo4ICyzCCAscwfQYI\n"
       "KwYBBQUHAQEEcTBvMEwGCCsGAQUFBzAChkBodHRwOi8vd3d3LmFjY3YuZXMvZmls\n"
       "ZWFkbWluL0FyY2hpdm9zL2NlcnRpZmljYWRvcy9yYWl6YWNjdjEuY3J0MB8GCCsG\n"
       "AQUFBzABhhNodHRwOi8vb2NzcC5hY2N2LmVzMB0GA1UdDgQWBBTSh7Tj3zcnk1X2\n"
       "VuqB5TbMjB4/vTAPBgNVHRMBAf8EBTADAQH/MB8GA1UdIwQYMBaAFNKHtOPfNyeT\n"
       "VfZW6oHlNsyMHj+9MIIBcwYDVR0gBIIBajCCAWYwggFiBgRVHSAAMIIBWDCCASIG\n"
       "CCsGAQUFBwICMIIBFB6CARAAQQB1AHQAbwByAGkAZABhAGQAIABkAGUAIABDAGUA\n"
       "cgB0AGkAZgBpAGMAYQBjAGkA8wBuACAAUgBhAO0AegAgAGQAZQAgAGwAYQAgAEEA\n"
       "QwBDAFYAIAAoAEEAZwBlAG4AYwBpAGEAIABkAGUAIABUAGUAYwBuAG8AbABvAGcA\n"
       "7QBhACAAeQAgAEMAZQByAHQAaQBmAGkAYwBhAGMAaQDzAG4AIABFAGwAZQBjAHQA\n"
       "cgDzAG4AaQBjAGEALAAgAEMASQBGACAAUQA0ADYAMAAxADEANQA2AEUAKQAuACAA\n"
       "QwBQAFMAIABlAG4AIABoAHQAdABwADoALwAvAHcAdwB3AC4AYQBjAGMAdgAuAGUA\n"
       "czAwBggrBgEFBQcCARYkaHR0cDovL3d3dy5hY2N2LmVzL2xlZ2lzbGFjaW9uX2Mu\n"
       "aHRtMFUGA1UdHwROMEwwSqBIoEaGRGh0dHA6Ly93d3cuYWNjdi5lcy9maWxlYWRt\n"
       "aW4vQXJjaGl2b3MvY2VydGlmaWNhZG9zL3JhaXphY2N2MV9kZXIuY3JsMA4GA1Ud\n"
       "DwEB/wQEAwIBBjAXBgNVHREEEDAOgQxhY2N2QGFjY3YuZXMwDQYJKoZIhvcNAQEF\n"
       "BQADggIBAJcxAp/n/UNnSEQU5CmH7UwoZtCPNdpNYbdKl02125DgBS4OxnnQ8pdp\n"
       "D70ER9m+27Up2pvZrqmZ1dM8MJP1jaGo/AaNRPTKFpV8M9xii6g3+CfYCS0b78gU\n"
       "JyCpZET/LtZ1qmxNYEAZSUNUY9rizLpm5U9EelvZaoErQNV/+QEnWCzI7UiRfD+m\n"
       "AM/EKXMRNt6GGT6d7hmKG9Ww7Y49nCrADdg9ZuM8Db3VlFzi4qc1GwQA9j9ajepD\n"
       "vV+JHanBsMyZ4k0ACtrJJ1vnE5Bc5PUzolVt3OAJTS+xJlsndQAJxGJ3KQhfnlms\n"
       "tn6tn1QwIgPBHnFk/vk4CpYY3QIUrCPLBhwepH2NDd4nQeit2hW3sCPdK6jT2iWH\n"
       "7ehVRE2I9DZ+hJp4rPcOVkkO1jMl1oRQQmwgEh0q1b688nCBpHBgvgW1m54ERL5h\n"
       "I6zppSSMEYCUWqKiuUnSwdzRp+0xESyeGabu4VXhwOrPDYTkF7eifKXeVSUG7szA\n"
       "h1xA2syVP1XgNce4hL60Xc16gwFy7ofmXx2utYXGJt/mwZrpHgJHnyqobalbz+xF\n"
       "d3+YJ5oyXSrjhO7FmGYvliAd3djDJ9ew+f7Zfc3Qn48LFFhRny+Lwzgt3uiP1o2H\n"
       "pPVWQxaZLPSkVrQ0uGE3ycJYgBugl6H8WY3pEfbRD0tVNEYqi4Y7\n"
       "-----END CERTIFICATE-----\n"
       "\n";
}

static void load_server_certificate(boost::asio::ssl::context& ctx,
                                    const http_config::pointer&) {
  /*
      The certificate was generated from bash on Ubuntu (OpenSSL 1.1.1f) using:

      openssl dhparam -out dh.pem 2048
      openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 10000 -out
     cert.pem -subj "/C=US/ST=CA/L=Los Angeles/O=Beast/CN=www.example.com"
  */

  std::string const cert =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIDlTCCAn2gAwIBAgIUOLxr3q7Wd/pto1+2MsW4fdRheCIwDQYJKoZIhvcNAQEL\n"
      "BQAwWjELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAkNBMRQwEgYDVQQHDAtMb3MgQW5n\n"
      "ZWxlczEOMAwGA1UECgwFQmVhc3QxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAe\n"
      "Fw0yMTA3MDYwMTQ5MjVaFw00ODExMjEwMTQ5MjVaMFoxCzAJBgNVBAYTAlVTMQsw\n"
      "CQYDVQQIDAJDQTEUMBIGA1UEBwwLTG9zIEFuZ2VsZXMxDjAMBgNVBAoMBUJlYXN0\n"
      "MRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IB\n"
      "DwAwggEKAoIBAQCz0GwgnxSBhygxBdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOT\n"
      "Lppr5MKxqQHEpYdyDYGD1noBoz4TiIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05\n"
      "P41c2F9pBEnUwUxIUG1Cb6AN0cZWF/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATm\n"
      "Wm9SJc2lsjWptbyIN6hFXLYPXTwnYzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7\n"
      "rV+AiGTxUU35V0AzpJlmvct5aJV/5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4\n"
      "lIzJ8yxzOzSStBPxvdrOobSSNlRZIlE7gnyNAgMBAAGjUzBRMB0GA1UdDgQWBBR+\n"
      "dYtY9zmFSw9GYpEXC1iJKHC0/jAfBgNVHSMEGDAWgBR+dYtY9zmFSw9GYpEXC1iJ\n"
      "KHC0/jAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBzKrsiYywl\n"
      "RKeB2LbddgSf7ahiQMXCZpAjZeJikIoEmx+AmjQk1bam+M7WfpRAMnCKooU+Utp5\n"
      "TwtijjnJydkZHFR6UH6oCWm8RsUVxruao/B0UFRlD8q+ZxGd4fGTdLg/ztmA+9oC\n"
      "EmrcQNdz/KIxJj/fRB3j9GM4lkdaIju47V998Z619E/6pt7GWcAySm1faPB0X4fL\n"
      "FJ6iYR2r/kJLoppPqL0EE49uwyYQ1dKhXS2hk+IIfA9mBn8eAFb/0435A2fXutds\n"
      "qhvwIOmAObCzcoKkz3sChbk4ToUTqbC0TmFAXI5Upz1wnADzjpbJrpegCA3pmvhT\n"
      "7356drqnCGY9\n"
      "-----END CERTIFICATE-----\n";

  std::string const key =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCz0GwgnxSBhygx\n"
      "BdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOTLppr5MKxqQHEpYdyDYGD1noBoz4T\n"
      "iIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05P41c2F9pBEnUwUxIUG1Cb6AN0cZW\n"
      "F/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATmWm9SJc2lsjWptbyIN6hFXLYPXTwn\n"
      "YzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7rV+AiGTxUU35V0AzpJlmvct5aJV/\n"
      "5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4lIzJ8yxzOzSStBPxvdrOobSSNlRZ\n"
      "IlE7gnyNAgMBAAECggEAY0RorQmldGx9D7M+XYOPjsWLs1px0cXFwGA20kCgVEp1\n"
      "kleBeHt93JqJsTKwOzN2tswl9/ZrnIPWPUpcbBlB40ggjzQk5k4jBY50Nk2jsxuV\n"
      "9A9qzrP7AoqhAYTQjZe42SMtbkPZhEeOyvCqxBAi6csLhcv4eB4+In0kQo7dfvLs\n"
      "Xu/3WhSsuAWqdD9EGnhD3n+hVTtgiasRe9318/3R9DzP+IokoQGOtXm+1dsfP0mV\n"
      "8XGzQHBpUtJNn0yi6SC4kGEQuKkX33zORlSnZgT5VBLofNgra0THd7x3atOx1lbr\n"
      "V0QizvCdBa6j6FwhOQwW8UwgOCnUbWXl/Xn4OaofMQKBgQDdRXSMyys7qUMe4SYM\n"
      "Mdawj+rjv0Hg98/xORuXKEISh2snJGKEwV7L0vCn468n+sM19z62Axz+lvOUH8Qr\n"
      "hLkBNqJvtIP+b0ljRjem78K4a4qIqUlpejpRLw6a/+44L76pMJXrYg3zdBfwzfwu\n"
      "b9NXdwHzWoNuj4v36teGP6xOUwKBgQDQCT52XX96NseNC6HeK5BgWYYjjxmhksHi\n"
      "stjzPJKySWXZqJpHfXI8qpOd0Sd1FHB+q1s3hand9c+Rxs762OXlqA9Q4i+4qEYZ\n"
      "qhyRkTsl+2BhgzxmoqGd5gsVT7KV8XqtuHWLmetNEi+7+mGSFf2iNFnonKlvT1JX\n"
      "4OQZC7ntnwKBgH/ORFmmaFxXkfteFLnqd5UYK5ZMvGKTALrWP4d5q2BEc7HyJC2F\n"
      "+5lDR9nRezRedS7QlppPBgpPanXeO1LfoHSA+CYJYEwwP3Vl83Mq/Y/EHgp9rXeN\n"
      "L+4AfjEtLo2pljjnZVDGHETIg6OFdunjkXDtvmSvnUbZBwG11bMnSAEdAoGBAKFw\n"
      "qwJb6FNFM3JnNoQctnuuvYPWxwM1yjRMqkOIHCczAlD4oFEeLoqZrNhpuP8Ij4wd\n"
      "GjpqBbpzyVLNP043B6FC3C/edz4Lh+resjDczVPaUZ8aosLbLiREoxE0udfWf2dU\n"
      "oBNnrMwwcs6jrRga7Kr1iVgUSwBQRAxiP2CYUv7tAoGBAKdPdekPNP/rCnHkKIkj\n"
      "o13pr+LJ8t+15vVzZNHwPHUWiYXFhG8Ivx7rqLQSPGcuPhNss3bg1RJiZAUvF6fd\n"
      "e6QS4EZM9dhhlO2FmPQCJMrRVDXaV+9TcJZXCbclQnzzBus9pwZZyw4Anxo0vmir\n"
      "nOMOU6XI4lO9Xge/QDEN4Y2R\n"
      "-----END PRIVATE KEY-----\n";

  std::string const dh =
      "-----BEGIN DH PARAMETERS-----\n"
      "MIIBCAKCAQEArzQc5mpm0Fs8yahDeySj31JZlwEphUdZ9StM2D8+Fo7TMduGtSi+\n"
      "/HRWVwHcTFAgrxVdm+dl474mOUqqaz4MpzIb6+6OVfWHbQJmXPepZKyu4LgUPvY/\n"
      "4q3/iDMjIS0fLOu/bLuObwU5ccZmDgfhmz1GanRlTQOiYRty3FiOATWZBRh6uv4u\n"
      "tff4A9Bm3V9tLx9S6djq31w31Gl7OQhryodW28kc16t9TvO1BzcV3HjRPwpe701X\n"
      "oEEZdnZWANkkpR/m/pfgdmGPU66S2sXMHgsliViQWpDCYeehrvFRHEdR9NV+XJfC\n"
      "QMUk26jPTIVTLfXmmwU0u8vUkpR7LQKkwwIBAg==\n"
      "-----END DH PARAMETERS-----\n";

  ctx.set_password_callback(
      [](std::size_t, boost::asio::ssl::context_base::password_purpose) {
        return "test";
      });

  ctx.set_options(boost::asio::ssl::context::default_workarounds |
                  boost::asio::ssl::context::no_sslv2 |
                  boost::asio::ssl::context::single_dh_use);

  ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));

  ctx.use_private_key(boost::asio::buffer(key.data(), key.size()),
                      boost::asio::ssl::context::file_format::pem);

  ctx.use_tmp_dh(boost::asio::buffer(dh.data(), dh.size()));
}

static std::shared_ptr<spdlog::logger> logger =
    spdlog::stdout_color_mt("http_connection_test");

/*********************************************************************
 * http keepalive test
 *********************************************************************/
class dummy_connection : public connection_base {
  asio::ip::tcp::socket _useless;

 public:
  void set_state(int state) { _state = state; }

  dummy_connection(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger,
                   const http_config::pointer& conf)
      : connection_base(io_context, logger, conf), _useless(*io_context) {}

  void shutdown() override { _state = e_not_connected; }

  void connect(connect_callback_type&& callback [[maybe_unused]]) override {}

  void send(request_ptr request [[maybe_unused]],
            send_callback_type&& callback [[maybe_unused]]) override {}

  void _on_accept(connect_callback_type&& callback [[maybe_unused]]) override{};
  void answer(const response_ptr& response [[maybe_unused]],
              answer_callback_type&& callback [[maybe_unused]]) override{};
  void receive_request(request_callback_type&& callback
                       [[maybe_unused]]) override{};

  asio::ip::tcp::socket& get_socket() override { return _useless; }
};

TEST(http_keepalive_test, ConnectionClose) {
  dummy_connection conn(
      g_io_context, logger,
      std::make_shared<http_config>(test_endpoint, "localhost"));
  response_ptr resp(std::make_shared<response_type>());
  resp->keep_alive(false);
  conn.set_state(connection_base::e_receive);
  conn.gest_keepalive(resp);
  ASSERT_EQ(conn.get_state(), connection_base::e_not_connected);
}

TEST(http_keepalive_test, KeepAliveWithoutTimeout) {
  auto conf = std::make_shared<http_config>(test_endpoint, "localhost");
  dummy_connection conn(g_io_context, logger, conf);
  response_ptr resp(std::make_shared<response_type>());
  resp->keep_alive(true);
  conn.set_state(connection_base::e_idle);
  conn.gest_keepalive(resp);
  time_point keepalive_end_expected =
      system_clock::now() + conf->get_default_http_keepalive_duration();
  ASSERT_EQ(conn.get_state(), connection_base::e_idle);
  ASSERT_LE(conn.get_keep_alive_end(),
            keepalive_end_expected + std::chrono::milliseconds(10));
  ASSERT_LE(keepalive_end_expected,
            conn.get_keep_alive_end() + std::chrono::milliseconds(10));
}

TEST(http_keepalive_test, KeepAliveWithTimeout) {
  auto conf = std::make_shared<http_config>(test_endpoint, "localhost");
  dummy_connection conn(g_io_context, logger, conf);
  response_ptr resp(std::make_shared<response_type>());
  resp->keep_alive(true);
  resp->set(beast::http::field::keep_alive, "timeout=5, max=1000");
  conn.set_state(connection_base::e_idle);
  conn.gest_keepalive(resp);
  time_point keepalive_end_expected =
      system_clock::now() + std::chrono::seconds(5);
  ASSERT_EQ(conn.get_state(), connection_base::e_idle);
  ASSERT_LE(conn.get_keep_alive_end(),
            keepalive_end_expected + std::chrono::milliseconds(10));
  ASSERT_LE(keepalive_end_expected,
            conn.get_keep_alive_end() + std::chrono::milliseconds(10));
}

/*********************************************************************
 * connection test
 *********************************************************************/

const char* client_cert_path = "/tmp/client_test.cert";

/**
 * @brief the template parameter indicate if test is played on https versus http
 *
 */
class http_test : public ::testing::TestWithParam<bool> {
 public:
  static void SetUpTestSuite() {
    create_client_certificate(client_cert_path);
    logger->set_level(spdlog::level::debug);
  };
  void SetUp() override {}

  void TearDown() override {}

  http_config::pointer create_conf() {
    if (GetParam()) {
      return std::make_shared<http_config>(
          test_endpoint, "localhost", true, std::chrono::seconds(10),
          std::chrono::seconds(10), std::chrono::seconds(10), 30,
          std::chrono::seconds(10), 5, std::chrono::hours(1), 10,
          asio::ssl::context_base::sslv23_client, client_cert_path);

    } else {
      return std::make_shared<http_config>(test_endpoint, "localhost", false);
    }
  }
  http_config::pointer create_server_conf() {
    if (GetParam()) {
      return std::make_shared<http_config>(
          listen_endpoint, "localhost", true, std::chrono::seconds(10),
          std::chrono::seconds(10), std::chrono::seconds(10), 30,
          std::chrono::seconds(10), 5, std::chrono::hours(1), 10,
          asio::ssl::context_base::sslv23_server);

    } else {
      return std::make_shared<http_config>(
          listen_endpoint, "localhost", false, std::chrono::seconds(10),
          std::chrono::seconds(10), std::chrono::seconds(10), 30,
          std::chrono::seconds(10), 5, std::chrono::hours(1), 10);
    }
  }
};

template <class base_class>
class answer_no_keep_alive : public base_class {
 public:
  using my_type = answer_no_keep_alive<base_class>;

  answer_no_keep_alive(const std::shared_ptr<asio::io_context>& io_context,
                       const std::shared_ptr<spdlog::logger>& logger,
                       const http_config::pointer& conf,
                       const ssl_ctx_initializer& ssl_initializer)
      : base_class(io_context, logger, conf, ssl_initializer) {}

  void on_accept() override {
    base_class::_on_accept([me = base_class::shared_from_this()](
                               const boost::beast::error_code ec,
                               const std::string&) {
      ASSERT_FALSE(ec);
      me->receive_request([me](const boost::beast::error_code ec,
                               const std::string&,
                               const std::shared_ptr<request_type>& request) {
        ASSERT_FALSE(ec);
        ASSERT_EQ(request->body(), "hello server");
        response_ptr resp(std::make_shared<response_type>(
            beast::http::status::ok, request->version()));
        resp->keep_alive(false);
        resp->body() = "hello client";
        resp->content_length(resp->body().length());
        me->answer(resp, [](const boost::beast::error_code ec,
                            const std::string&) { ASSERT_FALSE(ec); });
      });
    });
  }
  void add_keep_alive_to_server_response(const response_ptr& response
                                         [[maybe_unused]]) const override {}
};

TEST_P(http_test, connect_send_answer_without_keepalive) {
  std::shared_ptr<connection_base> conn;
  http_config::pointer client_conf = create_conf();
  http_config::pointer server_conf = create_server_conf();

  connection_creator server_creator;

  if (GetParam()) {  // crypted
    server_creator = [server_conf]() {
      return std::make_shared<answer_no_keep_alive<https_connection>>(
          g_io_context, logger, server_conf, load_server_certificate);
    };
  } else {
    server_creator = [server_conf]() {
      return std::make_shared<answer_no_keep_alive<http_connection>>(
          g_io_context, logger, server_conf, nullptr);
    };
  }

  auto client = GetParam()
                    ? https_connection::load(g_io_context, logger, client_conf)
                    : http_connection::load(g_io_context, logger, client_conf);
  request_ptr request(std::make_shared<request_base>());
  request->method(beast::http::verb::put);
  request->target("/");
  request->body() = "hello server";
  request->content_length(request->body().length());
  std::promise<std::tuple<beast::error_code, std::string, response_ptr>> p;
  auto f(p.get_future());
  time_point send_begin = system_clock::now();

  auto serv = server::load(g_io_context, logger, server_conf,
                           std::move(server_creator));

  client->connect([&p, client, request](const beast::error_code& err,
                                        const std::string& detail) {
    if (err) {
      p.set_value(std::make_tuple(err, detail, response_ptr()));
    } else {
      client->send(request,
                   [&p](const beast::error_code& err, const std::string& detail,
                        const response_ptr& response) mutable {
                     p.set_value(std::make_tuple(err, detail, response));
                   });
    }
  });

  auto completion = f.get();
  time_point send_end = system_clock::now();
  ASSERT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(send_end -
                                                                  send_begin),
            std::chrono::milliseconds(200));
  ASSERT_FALSE(std::get<0>(completion));
  ASSERT_TRUE(std::get<1>(completion).empty());
  ASSERT_EQ(std::get<2>(completion)->body(), "hello client");
  ASSERT_EQ(std::get<2>(completion)->keep_alive(), false);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ASSERT_EQ(client->get_state(), connection_base::e_not_connected);

  serv->shutdown();
}

// simple exchange with  keepalive
template <class base_class>
class answer_keep_alive : public base_class {
  unsigned _counter;

 public:
  using my_type = answer_keep_alive<base_class>;

  answer_keep_alive(const std::shared_ptr<asio::io_context>& io_context,
                    const std::shared_ptr<spdlog::logger>& logger,
                    const http_config::pointer& conf,
                    const ssl_ctx_initializer& ssl_initializer)
      : base_class(io_context, logger, conf, ssl_initializer), _counter(0) {}

  void on_accept() override {
    base_class::_on_accept([me = base_class::shared_from_this()](
                               const boost::beast::error_code ec,
                               const std::string&) {
      ASSERT_FALSE(ec);
      me->receive_request([me](const boost::beast::error_code ec,
                               const std::string&,
                               const std::shared_ptr<request_type>& request) {
        ASSERT_FALSE(ec);
        ASSERT_EQ(request->body(), "hello server");
        SPDLOG_LOGGER_DEBUG(logger, "request receiver => answer");
        std::static_pointer_cast<my_type>(me)->answer_to_request(request);
      });
    });
  }

  void answer_to_request(const std::shared_ptr<request_type>& request) {
    response_ptr resp(std::make_shared<response_type>(beast::http::status::ok,
                                                      request->version()));
    resp->keep_alive(true);
    resp->body() = fmt::format("hello client {}", _counter++);
    resp->content_length(resp->body().length());
    SPDLOG_LOGGER_DEBUG(logger, "answer to client");
    base_class::answer(resp, [me = base_class::shared_from_this()](
                                 const boost::beast::error_code ec,
                                 const std::string&) {
      ASSERT_FALSE(ec);
      me->receive_request([me](const boost::beast::error_code ec,
                               const std::string&,
                               const std::shared_ptr<request_type>& request) {
        if (ec) {
          return;
        }
        ASSERT_EQ(request->body(), "hello server");
        SPDLOG_LOGGER_DEBUG(logger, "request receiver => answer");
        std::static_pointer_cast<my_type>(me)->answer_to_request(request);
      });
    });
  }
};

TEST_P(http_test, connect_send_answer_with_keepalive) {
  std::shared_ptr<connection_base> conn;
  http_config::pointer client_conf = create_conf();
  http_config::pointer server_conf = create_server_conf();

  connection_creator server_creator;

  if (GetParam()) {  // crypted
    server_creator = [server_conf]() {
      return std::make_shared<answer_keep_alive<https_connection>>(
          g_io_context, logger, server_conf, load_server_certificate);
    };
  } else {
    server_creator = [server_conf]() {
      return std::make_shared<answer_keep_alive<http_connection>>(
          g_io_context, logger, server_conf, nullptr);
    };
  }

  auto client = GetParam()
                    ? https_connection::load(g_io_context, logger, client_conf)
                    : http_connection::load(g_io_context, logger, client_conf);
  request_ptr request(std::make_shared<request_base>());
  request->method(beast::http::verb::put);
  request->target("/");
  request->body() = "hello server";
  request->content_length(request->body().length());
  std::promise<std::tuple<beast::error_code, std::string, response_ptr>> p;
  auto f(p.get_future());
  time_point send_begin = system_clock::now();

  auto serv = server::load(g_io_context, logger, server_conf,
                           std::move(server_creator));

  client->connect([&p, client, request](const beast::error_code& err,
                                        const std::string& detail) {
    if (err) {
      p.set_value(std::make_tuple(err, detail, response_ptr()));
    } else {
      client->send(request,
                   [&p](const beast::error_code& err, const std::string& detail,
                        const response_ptr& response) mutable {
                     p.set_value(std::make_tuple(err, detail, response));
                   });
    }
  });

  auto completion = f.get();
  time_point send_end = system_clock::now();
  ASSERT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(send_end -
                                                                  send_begin),
            std::chrono::milliseconds(200));
  ASSERT_FALSE(std::get<0>(completion));
  ASSERT_TRUE(std::get<1>(completion).empty());
  ASSERT_EQ(std::get<2>(completion)->body(), "hello client 0");
  ASSERT_EQ(std::get<2>(completion)->keep_alive(), true);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(client->get_state(), connection_base::e_idle);

  std::promise<std::tuple<beast::error_code, std::string, response_ptr>> p2;
  auto f2(p2.get_future());
  client->send(request,
               [&p2](const beast::error_code& err, const std::string& detail,
                     const response_ptr& response) mutable {
                 p2.set_value(std::make_tuple(err, detail, response));
               });

  auto completion2 = f2.get();
  ASSERT_FALSE(std::get<0>(completion2));
  ASSERT_TRUE(std::get<1>(completion2).empty());
  ASSERT_EQ(std::get<2>(completion2)->body(), "hello client 1");
  ASSERT_EQ(std::get<2>(completion2)->keep_alive(), true);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(client->get_state(), connection_base::e_idle);

  serv->shutdown();
}

INSTANTIATE_TEST_SUITE_P(http_connection,
                         http_test,
                         testing::Values(false, true));
