/**
 * Copyright 2026 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>

#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/configuration/applier/connector.hh"
#include "com/centreon/engine/configuration/applier/contact.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/service.hh"
#include "com/centreon/engine/configuration/whitelist.hh"

#include "com/centreon/engine/modules/opentelemetry/conf_helper.hh"

#include "../helper.hh"
#include "../test_engine.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::modules::opentelemetry;

/**
 * @brief record every handler invocation and its arguments, so tests can
 * check what get_otel_commands extracted from the configuration
 */
struct handler_recorder {
  struct record {
    std::string cmd_name;
    std::string cmd_line;
    std::string service;
    uint64_t host_id;
    uint64_t service_id;
    uint32_t check_interval;
    uint32_t retry_interval;
    uint32_t max_attempts;
    std::string check_period;
    std::string tz;
    std::string global_event_handler;
    std::string event_handler;
  };

  bool ret = true;
  std::vector<record> records;

  bool operator()(const std::string& cmd_name,
                  const std::string& cmd_line,
                  const std::string& service,
                  uint64_t host_id,
                  uint64_t service_id,
                  uint32_t check_interval,
                  uint32_t retry_interval,
                  uint32_t max_attempts,
                  const std::string& check_period,
                  const std::string& tz,
                  const std::string& global_event_handler,
                  const std::string& event_handler,
                  const std::shared_ptr<spdlog::logger>& logger
                  [[maybe_unused]]) {
    records.push_back(record{cmd_name, cmd_line, service, host_id, service_id,
                             check_interval, retry_interval, max_attempts,
                             check_period, tz, global_event_handler,
                             event_handler});
    return ret;
  }

  void reset(bool new_ret = true) {
    ret = new_ret;
    records.clear();
  }
};

class conf_helper_test : public TestEngine {
 public:
  /**
   * @brief build the following configuration:
   * - otel connector "agent" (fake opentelemetry connector)
   * - host "test_host" (id 1) checked through the "agent" connector (hcmd1)
   * - service "test_svc" (host 1, service 2) checked through "agent" (cmd2!12)
   * - service "test_svc_2" (host 1, service 3) checked through "agent" (cmd3!13)
   * - service "test_svc_no_otel" (host 1, service 4) checked with a plain
   *   command (cmd4)
   * - host "plain_host" (id 5) checked with a plain command (hcmd)
   * - service "plain_svc" (host 5, service 6) checked with a plain command
   */
  void SetUp() override {
    timeperiod::timeperiods.clear();
    contact::contacts.clear();
    host::hosts.clear();
    host::hosts_by_id.clear();
    service::services.clear();
    service::services_by_id.clear();

    init_config_state();

    configuration::error_cnt err;

    // the otel connector, needed so commands with connector 'agent' become
    // forwarding commands to an otel command
    configuration::applier::connector conn_aply;
    configuration::Connector cnn;
    configuration::connector_helper cnn_hlp(&cnn);
    cnn.set_connector_name("agent");
    cnn.set_connector_line(
        "opentelemetry "
        "--processor=nagios_telegraf --extractor=attributes "
        "--host_path=resource_metrics.scope_metrics.data.data_points."
        "attributes.host "
        "--service_path=resource_metrics.scope_metrics.data.data_points."
        "attributes.service");
    conn_aply.add_object(cnn);

    configuration::applier::contact ct_aply;
    configuration::Contact ctct{new_pb_configuration_contact("admin", true)};
    ct_aply.add_object(ctct);
    ct_aply.expand_objects(pb_config);
    ct_aply.resolve_object(ctct, err);

    configuration::Host hst =
        new_pb_configuration_host("test_host", "admin", 1, "agent", 1);
    configuration::applier::host hst_aply;
    hst_aply.add_object(hst);
    hst_aply.resolve_object(hst, err);

    configuration::Service svc = new_pb_configuration_service(
        "test_host", "test_svc", "admin", 2, "agent", 2);
    configuration::applier::service svc_aply;
    svc_aply.add_object(svc);
    svc_aply.resolve_object(svc, err);

    configuration::Service svc2 = new_pb_configuration_service(
        "test_host", "test_svc_2", "admin", 3, "agent", 3);
    // distinct argument so both services get different expanded command lines
    svc2.set_check_command("cmd3!13");
    svc_aply.add_object(svc2);
    svc_aply.resolve_object(svc2, err);

    configuration::Service svc_no_otel = new_pb_configuration_service(
        "test_host", "test_svc_no_otel", "admin", 4, "", 4);
    svc_aply.add_object(svc_no_otel);
    svc_aply.resolve_object(svc_no_otel, err);

    configuration::Host plain_hst = new_pb_configuration_host("plain_host",
                                                              "admin", 5);
    hst_aply.add_object(plain_hst);
    hst_aply.resolve_object(plain_hst, err);

    configuration::Service plain_svc = new_pb_configuration_service(
        "plain_host", "plain_svc", "admin", 6, "", 6);
    svc_aply.add_object(plain_svc);
    svc_aply.resolve_object(plain_svc, err);
  }

  void TearDown() override { deinit_config_state(); }

  /**
   * @brief register a raw command by its name and command line
   */
  static void add_raw_command(const std::string& name,
                              const std::string& cmd_line) {
    configuration::Command cmd;
    configuration::command_helper cmd_hlp(&cmd);
    cmd.set_command_name(name);
    cmd.set_command_line(cmd_line);
    configuration::applier::command cmd_aply;
    cmd_aply.add_object(cmd);
  }
};

/**
 * @brief check that an unknown host name returns e_unknown_host and that the
 * handler is never called
 */
TEST_F(conf_helper_test, unknown_host) {
  handler_recorder recorder;
  whitelist_cache cache;

  e_get_otel_commands_ret ret = get_otel_commands(
      "nonexistent_host", std::move(recorder), cache,
      spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::unknown_host);
  ASSERT_TRUE(recorder.records.empty());
}

/**
 * @brief check that a host with no opentelemetry command returns
 * e_no_cma_service and that the handler is never called
 */
TEST_F(conf_helper_test, no_otel_command) {
  handler_recorder recorder;
  whitelist_cache cache;

  e_get_otel_commands_ret ret = get_otel_commands(
      "plain_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::no_cma_service);
  ASSERT_TRUE(recorder.records.empty());
}

/**
 * @brief check that the handler is called once for the host, once for each
 * otel service and never for the non otel service, with the expected
 * arguments
 */
TEST_F(conf_helper_test, host_and_otl_services_reported) {
  handler_recorder recorder;
  whitelist_cache cache;

  e_get_otel_commands_ret ret =
      get_otel_commands("test_host", std::move(recorder), cache,
                        spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 3U);

  // host record
  const handler_recorder::record& hst_rec = recorder.records[0];
  ASSERT_EQ(hst_rec.cmd_name, "hcmd1");
  ASSERT_EQ(hst_rec.cmd_line, "echo 0");
  ASSERT_TRUE(hst_rec.service.empty());
  ASSERT_EQ(hst_rec.host_id, 1U);
  ASSERT_EQ(hst_rec.service_id, 0U);

  // service records
  const handler_recorder::record& svc_rec = recorder.records[1];
  ASSERT_EQ(svc_rec.cmd_name, "cmd2!12");
  ASSERT_EQ(svc_rec.cmd_line, "echo 'output| metric=12;50;75'");
  ASSERT_EQ(svc_rec.service, "test_svc");
  ASSERT_EQ(svc_rec.host_id, 1U);
  ASSERT_EQ(svc_rec.service_id, 2U);

  const handler_recorder::record& svc2_rec = recorder.records[2];
  ASSERT_EQ(svc2_rec.cmd_name, "cmd3!13");
  ASSERT_EQ(svc2_rec.cmd_line, "echo 'output| metric=13;50;75'");
  ASSERT_EQ(svc2_rec.service, "test_svc_2");
  ASSERT_EQ(svc2_rec.host_id, 1U);
  ASSERT_EQ(svc2_rec.service_id, 3U);
}

/**
 * @brief check that check interval, retry interval, max check attempts, check
 * period and timezone given to the handler match the host/service objects
 */
TEST_F(conf_helper_test, handler_receives_check_parameters) {
  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());
  ASSERT_EQ(recorder.records.size(), 3U);

  std::shared_ptr<host> hst = host::hosts["test_host"];
  std::shared_ptr<service> svc = service::services_by_id[{1, 2}];
  std::shared_ptr<service> svc2 = service::services_by_id[{1, 3}];
  ASSERT_TRUE(hst);
  ASSERT_TRUE(svc);
  ASSERT_TRUE(svc2);

  const handler_recorder::record& hst_rec = recorder.records[0];
  ASSERT_EQ(hst_rec.check_interval, hst->check_interval());
  ASSERT_EQ(hst_rec.retry_interval, hst->retry_interval());
  ASSERT_EQ(hst_rec.max_attempts, hst->max_check_attempts());
  ASSERT_EQ(hst_rec.check_period, hst->check_period());
  ASSERT_EQ(hst_rec.tz, hst->get_timezone());

  const handler_recorder::record& svc_rec = recorder.records[1];
  ASSERT_EQ(svc_rec.check_interval, svc->check_interval());
  ASSERT_EQ(svc_rec.retry_interval, svc->retry_interval());
  ASSERT_EQ(svc_rec.max_attempts, svc->max_check_attempts());
  ASSERT_EQ(svc_rec.check_period, svc->check_period());
  ASSERT_EQ(svc_rec.tz, svc->get_timezone());

  const handler_recorder::record& svc2_rec = recorder.records[2];
  ASSERT_EQ(svc2_rec.check_interval, svc2->check_interval());
  ASSERT_EQ(svc2_rec.retry_interval, svc2->retry_interval());
  ASSERT_EQ(svc2_rec.max_attempts, svc2->max_check_attempts());
  ASSERT_EQ(svc2_rec.check_period, svc2->check_period());
  ASSERT_EQ(svc2_rec.tz, svc2->get_timezone());
}

/**
 * @brief check that when the handler returns false for every command,
 * get_otel_commands returns e_no_cma_service even though commands were found
 */
TEST_F(conf_helper_test, handler_returns_false) {
  handler_recorder recorder;
  recorder.ret = false;
  whitelist_cache cache;

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::no_cma_service);
  ASSERT_EQ(recorder.records.size(), 3U);
}

/**
 * @brief check that after get_otel_commands ran with an empty whitelist,
 * the whitelist cache contains one allowed entry per command examined
 */
TEST_F(conf_helper_test, whitelist_cache_filled) {
  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());

  ASSERT_EQ(cache.whitelist_instance_id,
            configuration::whitelist::instance().instance_id());
  // host + 2 otel services, the non otel service is not examined
  ASSERT_EQ(cache.data.size(), 3U);
  for (const auto& [line, allowed] : cache.data) {
    ASSERT_TRUE(allowed) << line;
  }
  ASSERT_TRUE(cache.data.contains("echo 0"));
  ASSERT_TRUE(cache.data.contains("echo 'output| metric=12;50;75'"));
  ASSERT_TRUE(cache.data.contains("echo 'output| metric=13;50;75'"));
}

/**
 * @brief check that a cached rejection is used without calling the whitelist
 * nor the handler: the host command is rejected from the cache while the
 * services still pass
 */
TEST_F(conf_helper_test, whitelist_cache_negative) {
  handler_recorder recorder;
  whitelist_cache cache;
  // the cache is only used when the whitelist instance id matches
  cache.whitelist_instance_id =
      configuration::whitelist::instance().instance_id();
  cache.data.emplace("echo 0", false);

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 2U);
  ASSERT_EQ(recorder.records[0].service, "test_svc");
  ASSERT_EQ(recorder.records[1].service, "test_svc_2");
}

/**
 * @brief check that the cache is cleared when the whitelist instance id
 * does not match anymore
 */
TEST_F(conf_helper_test, whitelist_cache_cleared_on_instance_change) {
  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());
  ASSERT_EQ(cache.data.size(), 3U);

  // simulate a whitelist reload
  cache.whitelist_instance_id =
      configuration::whitelist::instance().instance_id() + 1;
  cache.data.emplace("sentinel", false);

  recorder.reset();

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 3U);
  // the stale sentinel entry disappeared and the cache got rebuilt
  ASSERT_EQ(cache.data.size(), 3U);
  ASSERT_FALSE(cache.data.contains("sentinel"));
  ASSERT_EQ(cache.whitelist_instance_id,
            configuration::whitelist::instance().instance_id());
}

/**
 * @brief check that global and per host/service event handler command lines
 * are expanded and given to the handler, and that a disabled service event
 * handler is reported empty
 */
TEST_F(conf_helper_test, event_handlers) {
  pb_config.set_enable_event_handlers(true);
  pb_config.set_global_host_event_handler("gehcmd");
  pb_config.set_global_service_event_handler("gehsvc");

  add_raw_command("gehcmd", "echo GH");
  add_raw_command("gehsvc", "echo GS");
  add_raw_command("ehcmd", "echo HSTEH");
  add_raw_command("ehsvc", "echo SVCEH");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  std::shared_ptr<service> svc = service::services_by_id[{1, 2}];
  std::shared_ptr<service> svc2 = service::services_by_id[{1, 3}];
  ASSERT_TRUE(hst);
  ASSERT_TRUE(svc);
  ASSERT_TRUE(svc2);

  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(true);
  svc->set_event_handler("ehsvc");
  svc->set_event_handler_enabled(true);
  svc2->set_event_handler("ehsvc");
  svc2->set_event_handler_enabled(false);

  handler_recorder recorder;
  whitelist_cache cache;

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 3U);

  // host: global and host event handler are filled
  ASSERT_EQ(recorder.records[0].global_event_handler, "echo GH");
  ASSERT_EQ(recorder.records[0].event_handler, "echo HSTEH");

  // service: global and service event handlers are filled
  ASSERT_EQ(recorder.records[1].global_event_handler, "echo GS");
  ASSERT_EQ(recorder.records[1].event_handler, "echo SVCEH");

  // service with disabled event handler: only the global one is filled
  ASSERT_EQ(recorder.records[2].global_event_handler, "echo GS");
  ASSERT_TRUE(recorder.records[2].event_handler.empty());
}

/**
 * @brief check that when event handlers are disabled in the global
 * configuration, the global handler is not reported, but per host event
 * handlers are still extracted (they don't depend on the global switch)
 */
TEST_F(conf_helper_test, event_handlers_disabled_globally) {
  add_raw_command("gehcmd", "echo GH");
  add_raw_command("ehcmd", "echo HSTEH");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  ASSERT_TRUE(hst);
  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(true);
  // pb_config.enable_event_handlers() defaults to false, and the global
  // event handler is not set: no global handler is filled

  handler_recorder recorder;
  whitelist_cache cache;

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 3U);
  for (const handler_recorder::record& rec : recorder.records) {
    ASSERT_TRUE(rec.global_event_handler.empty());
  }
  // the per host event handler is extracted regardless
  ASSERT_EQ(recorder.records[0].event_handler, "echo HSTEH");
}

/**
 * @brief check that the state macros of kept_macros (HOSTSTATE, HOSTSTATETYPE,
 * HOSTATTEMPT for hosts and SERVICESTATE, SERVICESTATETYPE, SERVICEATTEMPT for
 * services) are not expanded by the engine but kept literal in the event
 * handler command lines: the agent resolves them itself when it executes the
 * event handler
 */
TEST_F(conf_helper_test, event_handler_state_macros_are_kept) {
  pb_config.set_enable_event_handlers(true);
  pb_config.set_global_host_event_handler("gehcmd");
  pb_config.set_global_service_event_handler("gehsvc");

  add_raw_command(
      "gehcmd", "echo $HOSTSTATE$ $HOSTSTATETYPE$ $HOSTATTEMPT$");
  add_raw_command("gehsvc",
                  "echo $SERVICESTATE$ $SERVICESTATETYPE$ $SERVICEATTEMPT$");
  add_raw_command("ehcmd", "echo $HOSTSTATE$-$HOSTATTEMPT$");
  add_raw_command("ehsvc", "echo $SERVICESTATE$-$SERVICEATTEMPT$");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  std::shared_ptr<service> svc = service::services_by_id[{1, 2}];
  std::shared_ptr<service> svc2 = service::services_by_id[{1, 3}];
  ASSERT_TRUE(hst);
  ASSERT_TRUE(svc);
  ASSERT_TRUE(svc2);

  // if the engine expanded these macros, the following values would show up
  // in the command lines, this is exactly what must not happen
  hst->set_current_state(host::state_down);
  hst->set_state_type(checkable::soft);
  hst->set_current_attempt(2);
  svc->set_current_state(service::state_warning);
  svc->set_state_type(checkable::soft);
  svc->set_current_attempt(1);

  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(true);
  svc->set_event_handler("ehsvc");
  svc->set_event_handler_enabled(true);
  svc2->set_event_handler("ehsvc");
  svc2->set_event_handler_enabled(false);

  handler_recorder recorder;
  whitelist_cache cache;

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 3U);

  // host: the state macros are kept as is
  ASSERT_EQ(recorder.records[0].global_event_handler,
            "echo $HOSTSTATE$ $HOSTSTATETYPE$ $HOSTATTEMPT$");
  ASSERT_EQ(recorder.records[0].event_handler,
            "echo $HOSTSTATE$-$HOSTATTEMPT$");

  // service: same for the service state macros
  ASSERT_EQ(recorder.records[1].global_event_handler,
            "echo $SERVICESTATE$ $SERVICESTATETYPE$ $SERVICEATTEMPT$");
  ASSERT_EQ(recorder.records[1].event_handler,
            "echo $SERVICESTATE$-$SERVICEATTEMPT$");

  // service with disabled event handler: the state macros don't leak in
  ASSERT_TRUE(recorder.records[2].event_handler.empty());
}

/**
 * @brief check that an event handler naming an unknown command is reported
 * empty while the check command is still given to the handler
 */
TEST_F(conf_helper_test, event_handler_unknown_command) {
  std::shared_ptr<host> hst = host::hosts["test_host"];
  std::shared_ptr<service> svc = service::services_by_id[{1, 2}];
  ASSERT_TRUE(hst);
  ASSERT_TRUE(svc);
  hst->set_event_handler("no_such_command");
  hst->set_event_handler_enabled(true);
  svc->set_event_handler("no_such_command");
  svc->set_event_handler_enabled(true);

  handler_recorder recorder;
  whitelist_cache cache;

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 3U);
  ASSERT_TRUE(recorder.records[0].event_handler.empty());
  ASSERT_EQ(recorder.records[0].cmd_line, "echo 0");
  ASSERT_TRUE(recorder.records[1].event_handler.empty());
  ASSERT_EQ(recorder.records[1].cmd_line, "echo 'output| metric=12;50;75'");
}

/**
 * @brief check that a host event handler that is set but disabled is reported
 * empty
 */
TEST_F(conf_helper_test, host_event_handler_disabled) {
  add_raw_command("ehcmd", "echo HSTEH");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  ASSERT_TRUE(hst);
  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(false);

  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());

  ASSERT_EQ(recorder.records.size(), 3U);
  ASSERT_TRUE(recorder.records[0].event_handler.empty());
}

/**
 * @brief check that global event handlers are ignored when event handlers are
 * disabled in the global configuration, even if they are set
 */
TEST_F(conf_helper_test, global_event_handlers_set_but_disabled) {
  pb_config.set_enable_event_handlers(false);
  pb_config.set_global_host_event_handler("gehcmd");
  pb_config.set_global_service_event_handler("gehsvc");
  add_raw_command("gehcmd", "echo GH");
  add_raw_command("gehsvc", "echo GS");

  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());

  ASSERT_EQ(recorder.records.size(), 3U);
  for (const handler_recorder::record& rec : recorder.records) {
    ASSERT_TRUE(rec.global_event_handler.empty()) << rec.cmd_name;
  }
}

/**
 * @brief check that the global host event handler is not given to the
 * services when no global service event handler is configured
 */
TEST_F(conf_helper_test, global_host_event_handler_not_given_to_services) {
  pb_config.set_enable_event_handlers(true);
  pb_config.set_global_host_event_handler("gehcmd");
  add_raw_command("gehcmd", "echo GH");

  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());

  ASSERT_EQ(recorder.records.size(), 3U);
  ASSERT_EQ(recorder.records[0].global_event_handler, "echo GH");
  ASSERT_TRUE(recorder.records[1].global_event_handler.empty());
  ASSERT_TRUE(recorder.records[2].global_event_handler.empty());
}

/**
 * @brief check that the host event handler is not given to the services
 * that have no event handler
 */
TEST_F(conf_helper_test, host_event_handler_not_given_to_services) {
  add_raw_command("ehcmd", "echo HSTEH");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  ASSERT_TRUE(hst);
  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(true);

  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());

  ASSERT_EQ(recorder.records.size(), 3U);
  ASSERT_EQ(recorder.records[0].event_handler, "echo HSTEH");
  ASSERT_TRUE(recorder.records[1].event_handler.empty());
  ASSERT_TRUE(recorder.records[2].event_handler.empty());
}

/**
 * @brief check that an event handler rejected by the whitelist is reported
 * empty, while the check command is still given to the handler
 */
TEST_F(conf_helper_test, event_handler_rejected_by_whitelist) {
  pb_config.set_enable_event_handlers(true);
  pb_config.set_global_service_event_handler("gehsvc");
  add_raw_command("gehsvc", "echo GS");
  add_raw_command("ehcmd", "echo HSTEH");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  ASSERT_TRUE(hst);
  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(true);

  handler_recorder recorder;
  whitelist_cache cache;
  cache.whitelist_instance_id =
      configuration::whitelist::instance().instance_id();
  cache.data.emplace("echo HSTEH", false);
  cache.data.emplace("echo GS", false);

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 3U);
  ASSERT_EQ(recorder.records[0].cmd_line, "echo 0");
  ASSERT_TRUE(recorder.records[0].event_handler.empty());
  ASSERT_TRUE(recorder.records[1].global_event_handler.empty());
  ASSERT_TRUE(recorder.records[2].global_event_handler.empty());
}

/**
 * @brief check that a service check command rejected by the whitelist is not
 * given to the handler, while the other commands are
 */
TEST_F(conf_helper_test, service_check_rejected_by_whitelist) {
  handler_recorder recorder;
  whitelist_cache cache;
  cache.whitelist_instance_id =
      configuration::whitelist::instance().instance_id();
  cache.data.emplace("echo 'output| metric=12;50;75'", false);

  e_get_otel_commands_ret ret = get_otel_commands(
      "test_host", std::move(recorder), cache, spdlog::default_logger());

  ASSERT_EQ(ret, e_get_otel_commands_ret::success);
  ASSERT_EQ(recorder.records.size(), 2U);
  ASSERT_TRUE(recorder.records[0].service.empty());
  ASSERT_EQ(recorder.records[1].service, "test_svc_2");
}

/**
 * @brief check that the macros not kept for the agent (host name, service
 * description) are expanded in the event handler command lines
 */
TEST_F(conf_helper_test, event_handler_other_macros_expanded) {
  pb_config.set_enable_event_handlers(true);
  pb_config.set_global_service_event_handler("gehsvc");
  add_raw_command("gehsvc", "echo $HOSTNAME$ $SERVICEDESC$ $SERVICESTATE$");
  add_raw_command("ehcmd", "echo $HOSTNAME$ $HOSTSTATE$");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  ASSERT_TRUE(hst);
  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(true);

  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());

  ASSERT_EQ(recorder.records.size(), 3U);
  ASSERT_EQ(recorder.records[0].event_handler, "echo test_host $HOSTSTATE$");
  ASSERT_EQ(recorder.records[1].global_event_handler,
            "echo test_host test_svc $SERVICESTATE$");
  ASSERT_EQ(recorder.records[2].global_event_handler,
            "echo test_host test_svc_2 $SERVICESTATE$");
}

/**
 * @brief check that the event handler command lines are recorded in the
 * whitelist cache next to the check command lines
 */
TEST_F(conf_helper_test, whitelist_cache_contains_event_handlers) {
  pb_config.set_enable_event_handlers(true);
  pb_config.set_global_service_event_handler("gehsvc");
  add_raw_command("gehsvc", "echo GS");
  add_raw_command("ehcmd", "echo HSTEH");

  std::shared_ptr<host> hst = host::hosts["test_host"];
  ASSERT_TRUE(hst);
  hst->set_event_handler("ehcmd");
  hst->set_event_handler_enabled(true);

  handler_recorder recorder;
  whitelist_cache cache;

  get_otel_commands("test_host", std::move(recorder), cache,
                    spdlog::default_logger());

  // 3 check commands + host event handler + global service event handler
  ASSERT_EQ(cache.data.size(), 5U);
  ASSERT_TRUE(cache.data.contains("echo HSTEH"));
  ASSERT_TRUE(cache.data.contains("echo GS"));
}
