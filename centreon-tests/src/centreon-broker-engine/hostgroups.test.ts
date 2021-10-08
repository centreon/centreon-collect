import shell from "shelljs";
import { Broker, BrokerType } from "../core/broker";
import { Engine } from "../core/engine";
import { isBrokerAndEngineConnected } from "../core/brokerEngine";

shell.config.silent = true;

describe("engine reloads with new hosts and hostgroups configurations", () => {
  beforeEach(async () => {
    await Engine.cleanAllInstances();
    await Broker.cleanAllInstances();
    Broker.startMysql();

    Broker.clearLogs(BrokerType.central);
    Broker.clearRetention(BrokerType.central);
    Broker.clearRetention(BrokerType.module);
    Broker.resetConfig(BrokerType.central);

    const config_broker = await Broker.getConfig(BrokerType.central);
    config_broker["centreonBroker"]["log"]["loggers"]["sql"] = "trace";
    await Broker.writeConfig(BrokerType.central, config_broker);

    Engine.clearLogs();

    if (Broker.isInstancesRunning() || Engine.isInstancesRunning()) {
      console.log("program could not stop cbd or centengine");
      process.exit(1);
    }
  });

  afterAll(() => {
    beforeEach(async () => {
      await Engine.cleanAllInstances();
      await Broker.cleanAllInstances();
    });
  });

  it("New host group", async () => {
    const broker = new Broker(2);
    const engine = new Engine(2);
    await Engine.buildConfigs();
    const started1 = await broker.start();
    const started2 = await engine.start();

    const checklog1 = await engine.checkLogFileContains(
      0,
      [
        "Event broker module '/usr/lib64/nagios/cbmod.so' initialized successfully",
      ],
      120
    );

    const checklog2 = await engine.checkLogFileContains(
      1,
      [
        "Event broker module '/usr/lib64/nagios/cbmod.so' initialized successfully",
      ],
      120
    );

    const connected1 = await isBrokerAndEngineConnected();

    const hostgroupAdded1 = await engine.addHostgroup(1, [
      "host_1",
      "host_2",
      "host_3",
    ]);
    let p = [engine.reload(), broker.reload()];
    await Promise.all(p);

    const checkLog4 = await engine.checkLogFileContains(
      0,
      [
        "Event broker module '/usr/lib64/nagios/cbmod.so' initialized successfully",
      ],
      120
    );

    const connected2 = await isBrokerAndEngineConnected();

    const checkLog3 = await broker.checkCentralLogContains(
      [
        "enabling membership of host 3 to host group 1 on instance 1",
        "enabling membership of host 2 to host group 1 on instance 1",
        "enabling membership of host 1 to host group 1 on instance 1",
      ],
      60
    );

    const stopped1: boolean = await engine.stop();
    const stopped2: boolean = await broker.stop();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(checklog1).toBeTruthy();
    expect(checklog2).toBeTruthy();
    expect(connected1).toBeTruthy();
    expect(hostgroupAdded1).toBeTruthy();
    expect(checkLog4).toBeTruthy();
    expect(checkLog3).toBeTruthy();
    expect(connected2).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
  }, 120000);

  it("Many New host groups", async () => {
    const broker = new Broker(2);
    const engine = new Engine(2);
    await Engine.buildConfigs();
    const started1 = await broker.start();
    const started2 = await engine.start();

    const checkLog1 = await engine.checkLogFileContains(
      0,
      [
        "Event broker module '/usr/lib64/nagios/cbmod.so' initialized successfully",
      ],
      120
    );

    const checkLog2 = await engine.checkLogFileContains(
      1,
      [
        "Event broker module '/usr/lib64/nagios/cbmod.so' initialized successfully",
      ],
      120
    );

    const connected1 = await isBrokerAndEngineConnected();

    let hostnames: string[] = ["host_1", "host_2"];
    let logs: string[] = [];

    for (let i = 0; i < 2; i++) {
      let host = await engine.addHost(i % 2);
      hostnames.push(host.name);
      let group = await engine.addHostgroup(i + 3, hostnames);
      logs.push(
        `SQL: enabling membership of host ${host.id} to host group ${group.id} on instance 1`
      );
      logs.push(
        `SQL: processing host event (poller: 1, host: ${host.id}, name: ${host.name}`
      );
    }

    let p = [engine.reload(), broker.reload()];
    await Promise.all(p);
    const connected3 = await isBrokerAndEngineConnected();

    const checkLog5 = await broker.checkCentralLogContains(logs, 60);

    const stopped1 = await engine.stop();
    const stopped2 = await broker.stop();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(checkLog2).toBeTruthy();
    expect(connected1).toBeTruthy();
    expect(connected3).toBeTruthy();
    expect(checkLog5).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
  }, 120000);
});
