import sleep from "await-sleep";
import shell from "shelljs";
import { once } from "events";
import { Broker, BrokerType } from "../core/broker";
import { Engine } from "../core/engine";
import { isBrokerAndEngineConnected } from "../core/brokerEngine";
import { broker } from "shared";

import util from "util";
import process from "process";
import { rmSync } from "fs";

shell.config.silent = true;

describe("engine and broker testing in same time for compression", () => {
  beforeEach(() => {
    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    Broker.clearLogs(BrokerType.central);
    Broker.clearLogs(BrokerType.module);
    Broker.resetConfig(BrokerType.central);
    Broker.resetConfig(BrokerType.module);
    Broker.resetConfig(BrokerType.rrd);

    if (Broker.isServiceRunning() || Engine.isServiceRunning()) {
      console.log("program could not stop cbd or centengine");
      process.exit(1);
    }
  }, 30000);

  afterAll(() => {
    beforeEach(() => {
      Broker.clearLogs(BrokerType.central);
      Broker.resetConfig(BrokerType.central);
      Broker.resetConfig(BrokerType.module);
      Broker.resetConfig(BrokerType.rrd);
    });
  }, 30000);

  it("TLS without keys checks between broker - engine", async () => {
    const broker = new Broker();
    const engine = new Engine();
    await Engine.buildConfigs();
    await Engine.installConfigs();

    let tls = {
      yes: "TLS",
      no: "",
      auto: "TLS",
    };

    const config_broker = await Broker.getConfig(BrokerType.central);
    const config_module = await Broker.getConfig(BrokerType.module);

    const centralModuleLoggers =
      config_module["centreonBroker"]["log"]["loggers"];
    const centralBrokerLoggers =
      config_broker["centreonBroker"]["log"]["loggers"];

    centralModuleLoggers["bbdo"] = "info";
    centralBrokerLoggers["bbdo"] = "info";

    const centralModuleMaster = config_module["centreonBroker"]["output"].find(
      (output) => output.name === "central-module-master-output"
    );
    centralModuleMaster["compression"] = "no";
    const centralBrokerMaster = config_broker["centreonBroker"]["input"].find(
      (input) => input.name === "central-broker-master-input"
    );
    centralBrokerMaster["compression"] = "no";

    for (let t1 in tls) {
      for (let t2 in tls) {
        Broker.clearLogs(BrokerType.central);
        Broker.clearLogs(BrokerType.module);

        // Central
        centralBrokerMaster["tls"] = t1;

        // Module
        centralModuleMaster["tls"] = t2;

        let central: string[] = [
          `[bbdo] [info] BBDO: we have extensions '${tls[t1]}' and peer has '${tls[t2]}'`,
        ];
        let module: string[] = [
          `[bbdo] [info] BBDO: we have extensions '${tls[t2]}' and peer has '${tls[t1]}'`,
        ];

        if (t1 == "yes" && t2 == "no")
          central.push(
            "[bbdo] [error] BBDO: extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration."
          );
        else if (t1 == "no" && t2 == "yes")
          module.push(
            "[bbdo] [error] BBDO: extension 'TLS' is set to 'yes' in the configuration but cannot be activated because of peer configuration."
          );

        console.log(centralBrokerMaster);
        console.log(centralModuleMaster);

        await Broker.writeConfig(BrokerType.module, config_module);
        await Broker.writeConfig(BrokerType.central, config_broker);

        const started1 = await broker.start();
        const started2 = await engine.start();

        let connected = false;
        let checkLog1 = false;
        let checkLog2 = false;
        let stopped1 = false;
        let stopped2 = false;

        if (started1 && started2) {
          connected = await isBrokerAndEngineConnected();

          checkLog1 = await broker.checkCentralLogContains(central);
          checkLog2 = await broker.checkModuleLogContains(module);

          stopped1 = await broker.stop();
          stopped2 = await engine.stop();
        }
        Broker.cleanAllInstances();
        Engine.cleanAllInstances();

        expect(started1).toBeTruthy();
        expect(started2).toBeTruthy();
        expect(connected).toBeTruthy();
        expect(checkLog1).toBeTruthy();
        expect(checkLog2).toBeTruthy();
        expect(stopped1).toBeTruthy();
        expect(stopped2).toBeTruthy();
      }
    }
  }, 400000);

  it("tls with keys checks between broker - engine", async () => {
    const broker = new Broker();
    const engine = new Engine();

    const config_broker = await Broker.getConfig(BrokerType.central);
    const config_rrd = await Broker.getConfig(BrokerType.rrd);

    const centralBrokerLoggers =
      config_broker["centreonBroker"]["log"]["loggers"];

    centralBrokerLoggers["bbdo"] = "info";
    centralBrokerLoggers["tcp"] = "info";
    centralBrokerLoggers["tls"] = "trace";

    var centralBrokerMaster = config_broker["centreonBroker"]["output"].find(
      (output) => output.name === "centreon-broker-master-rrd"
    );
    const centralRrdMaster = config_rrd["centreonBroker"]["input"].find(
      (input) => input.name === "central-rrd-master-input"
    );

    // get hostname
    const output = shell.exec("hostname --fqdn");
    const hostname = output.stdout.replace(/\n/g, "");

    // generate keys
    shell.exec(
      "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/server.key -out /etc/centreon-broker/server.crt -subj '/CN='" +
        hostname
    );
    shell.exec(
      "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/client.key -out /etc/centreon-broker/client.crt -subj '/CN='" +
        hostname
    );

    // update configuration file
    centralBrokerMaster["tls"] = "yes";
    centralBrokerMaster["private_key"] = "/etc/centreon-broker/client.key";
    centralBrokerMaster["public_cert"] = "/etc/centreon-broker/client.crt";
    centralBrokerMaster["ca_certificate"] = "/etc/centreon-broker/server.crt";

    centralRrdMaster["tls"] = "yes";
    centralRrdMaster["private_key"] = "/etc/centreon-broker/server.key";
    centralRrdMaster["public_cert"] = "/etc/centreon-broker/server.crt";
    centralRrdMaster["ca_certificate"] = "/etc/centreon-broker/client.crt";

    // write changes in config_broker
    await Broker.writeConfig(BrokerType.central, config_broker);
    await Broker.writeConfig(BrokerType.rrd, config_rrd);

    console.log(centralBrokerMaster);
    console.log(centralRrdMaster);

    // starts centreon
    const started1: boolean = await broker.start();
    const started2: boolean = await engine.start();

    let connected: boolean;
    let checkLog1: boolean = false;
    let stopped1: boolean = false;
    let stopped2: boolean = false;

    if (started1 && started2) {
      connected = await isBrokerAndEngineConnected();

      // checking logs
      checkLog1 = await broker.checkCentralLogContains([
        "[tls] [info] TLS: using certificates as credentials",
        "[tls] [debug] TLS: performing handshake",
        "[tls] [debug] TLS: successful handshake",
      ]);

      stopped1 = await broker.stop();
      stopped2 = await engine.stop();
    }

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    rmSync("/etc/centreon-broker/client.key");
    rmSync("/etc/centreon-broker/client.crt");
    rmSync("/etc/centreon-broker/server.key");
    rmSync("/etc/centreon-broker/server.crt");

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
  }, 90000);

  it("Anonymous TLS with CA between broker - engine", async () => {
    const broker = new Broker();
    const engine = new Engine();

    const config_broker = await Broker.getConfig(BrokerType.central);
    const config_rrd = await Broker.getConfig(BrokerType.rrd);

    const centralBrokerLoggers =
      config_broker["centreonBroker"]["log"]["loggers"];

    centralBrokerLoggers["bbdo"] = "info";
    centralBrokerLoggers["tcp"] = "info";
    centralBrokerLoggers["tls"] = "trace";

    var centralBrokerMaster = config_broker["centreonBroker"]["output"].find(
      (output) => output.name === "centreon-broker-master-rrd"
    );
    const centralRrdMaster = config_rrd["centreonBroker"]["input"].find(
      (input) => input.name === "central-rrd-master-input"
    );

    // get hostname
    const output = shell.exec("hostname --fqdn");
    const hostname = output.stdout.replace(/\n/g, "");

    // generates keys
    shell.exec(
      "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -out /etc/centreon-broker/server.crt -subj '/CN='" +
        hostname
    );
    shell.exec(
      "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -out /etc/centreon-broker/client.crt -subj '/CN='" +
        hostname
    );

    // update configuration file
    centralBrokerMaster["tls"] = "yes";
    centralBrokerMaster["ca_certificate"] = "/etc/centreon-broker/server.crt";

    centralRrdMaster["tls"] = "yes";
    centralRrdMaster["ca_certificate"] = "/etc/centreon-broker/client.crt";

    // write changes in config_broker
    await Broker.writeConfig(BrokerType.central, config_broker);
    await Broker.writeConfig(BrokerType.rrd, config_rrd);

    console.log(centralBrokerMaster);
    console.log(centralRrdMaster);

    // starts centreon
    const started1: boolean = await broker.start();
    const started2: boolean = await engine.start();

    let connected: boolean;
    let checkLog1: boolean = false;
    let stopped1: boolean = false;
    let stopped2: boolean = false;

    if (started1 && started2) {
      connected = await isBrokerAndEngineConnected();

      // checking logs
      checkLog1 = await broker.checkCentralLogContains([
        "[tls] [info] TLS: using anonymous client credentials",
        "[tls] [debug] TLS: performing handshake",
        "[tls] [debug] TLS: successful handshake",
      ]);

      stopped1 = await broker.stop();
      stopped2 = await engine.stop();
    }

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    rmSync("/etc/centreon-broker/client.crt");
    rmSync("/etc/centreon-broker/server.crt");

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
  }, 90000);

  it("TLS with keys checks between broker - engine (common name)", async () => {
    const broker = new Broker();
    const engine = new Engine();

    const config_broker = await Broker.getConfig(BrokerType.central);
    const config_rrd = await Broker.getConfig(BrokerType.rrd);

    const centralBrokerLoggers =
      config_broker["centreonBroker"]["log"]["loggers"];

    centralBrokerLoggers["bbdo"] = "info";
    centralBrokerLoggers["tcp"] = "info";
    centralBrokerLoggers["tls"] = "trace";

    var centralBrokerMaster = config_broker["centreonBroker"]["output"].find(
      (output) => output.name === "centreon-broker-master-rrd"
    );
    const centralRrdMaster = config_rrd["centreonBroker"]["input"].find(
      (input) => input.name === "central-rrd-master-input"
    );

    // get hostname
    const output = await shell.exec("hostname --fqdn");

    // generates keys
    shell.exec(
      "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/server.key -out /etc/centreon-broker/server.crt -subj '/CN=centreon'"
    );
    shell.exec(
      "openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/client.key -out /etc/centreon-broker/client.crt -subj '/CN=centreon'"
    );

    // update configuration file
    centralBrokerMaster["tls"] = "yes";
    centralBrokerMaster["private_key"] = "/etc/centreon-broker/client.key";
    centralBrokerMaster["public_cert"] = "/etc/centreon-broker/client.crt";
    centralBrokerMaster["ca_certificate"] = "/etc/centreon-broker/server.crt";
    centralBrokerMaster["tls_hostname"] = "centreon";

    centralRrdMaster["tls"] = "yes";
    centralRrdMaster["private_key"] = "/etc/centreon-broker/server.key";
    centralRrdMaster["public_cert"] = "/etc/centreon-broker/server.crt";
    centralRrdMaster["ca_certificate"] = "/etc/centreon-broker/client.crt";

    // write changes in config_broker
    await Broker.writeConfig(BrokerType.central, config_broker);
    await Broker.writeConfig(BrokerType.rrd, config_rrd);

    console.log(centralBrokerMaster);
    console.log(centralRrdMaster);

    // starts centreon
    const started1: boolean = await broker.start();
    const started2: boolean = await engine.start();

    let connected: boolean;
    let checkLog1: boolean = false;
    let stopped1: boolean = false;
    let stopped2: boolean = false;

    if (started1 && started2) {
      connected = await isBrokerAndEngineConnected();

      // checking logs
      checkLog1 = await broker.checkCentralLogContains([
        "[tls] [info] TLS: using certificates as credentials",
        "[tls] [debug] TLS: performing handshake",
        "[tls] [debug] TLS: performing handshake",
        "[tls] [info] TLS: common name 'centreon' used for certificate verification",
      ]);

      stopped1 = await broker.stop();
      stopped2 = await engine.stop();
    }

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    rmSync("/etc/centreon-broker/client.key");
    rmSync("/etc/centreon-broker/client.crt");
    rmSync("/etc/centreon-broker/server.key");
    rmSync("/etc/centreon-broker/server.crt");

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected).toBeTruthy();
  }, 90000);
});
