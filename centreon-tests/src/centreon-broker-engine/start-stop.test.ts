import shell from "shelljs";
import { Broker, BrokerType } from "../core/broker";
import { Engine } from "../core/engine";
import { isBrokerAndEngineConnected } from "../core/brokerEngine";
import sleep from "await-sleep";
import { readFileSync } from "fs";
shell.config.silent = true;

describe("engine and broker testing in same time", () => {
  beforeEach(async () => {
    Broker.startMysql();
    await Engine.cleanAllInstances();
    await Broker.cleanAllInstances();

    Broker.clearLogs(BrokerType.central);
    Engine.clearLogs();
    Broker.resetConfig(BrokerType.central);
    Broker.resetConfig(BrokerType.module);
    Broker.resetConfig(BrokerType.rrd);

    if (Broker.isInstancesRunning() || Engine.isInstancesRunning()) {
      console.log("program could not stop cbd or centengine");
      process.exit(1);
    }
  }, 30000);

  it("BESS1: start/stop centreon broker/engine - start: broker first, stop: broker first", async () => {
    console.log("BESS1");
    const broker = new Broker();
    const engine = new Engine();
    Engine.buildConfigs();

    const started1 = await broker.start();
    const started2 = await engine.start();
    const connected = await isBrokerAndEngineConnected();

    const stopped1 = await broker.stop();
    const stopped2 = await engine.stop();

    const cd1 = await broker.checkCoredump();
    const cd2 = await engine.checkCoredump();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
    expect(cd1).toBeFalsy();
    expect(cd2).toBeFalsy();
  }, 60000);

  it("BESS2: start/stop centreon broker/engine - start: broker first, stop: engine first", async () => {
    console.log("BESS2");
    const broker = new Broker();
    const engine = new Engine();
    Engine.buildConfigs();

    const started1 = await broker.start();
    const started2 = await engine.start();
    const connected1 = await isBrokerAndEngineConnected();

    const stopped2 = await engine.stop();
    const cd2 = await engine.checkCoredump();

    const running1 = await broker.isRunning(2, 5);

    const started3 = await engine.start();
    const connected2 = await isBrokerAndEngineConnected();

    const stopped1 = await broker.stop();
    const stopped3 = await engine.stop();

    const cd1 = await broker.checkCoredump();
    const cd3 = await engine.checkCoredump();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected1).toBeTruthy();
    expect(stopped2).toBeTruthy();
    expect(cd2).toBeFalsy();
    expect(running1).toBeTruthy();
    expect(started3).toBeTruthy();
    expect(connected2).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped3).toBeTruthy();
    expect(cd1).toBeFalsy();
    expect(cd3).toBeFalsy();
  }, 60000);

  it("BESS3: start/stop centreon broker/engine - engine first", async () => {
    console.log("BESS3");
    const broker = new Broker(1);
    const engine = new Engine();

    const started1 = await engine.start();
    const started2 = await broker.start();
    const connected1 = await isBrokerAndEngineConnected();

    const stopped2 = await broker.stop();
    const cd2 = await broker.checkCoredump();

    const running1 = await engine.isRunning(2, 5);

    const started3 = await broker.start();
    const connected2 = await isBrokerAndEngineConnected();

    const stopped1 = await engine.stop();
    const stopped3 = await broker.stop();

    const cd1 = await engine.checkCoredump();
    const cd3 = await broker.checkCoredump();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected1).toBeTruthy();
    expect(stopped2).toBeTruthy();
    expect(cd2).toBeFalsy();
    expect(running1).toBeTruthy();
    expect(started3).toBeTruthy();
    expect(connected2).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped3).toBeTruthy();
    expect(cd1).toBeFalsy();
    expect(cd3).toBeFalsy();
  }, 100000);

  it("BEOPR1: start/stop centreon broker/engine - broker first", async () => {
    console.log("BEOPR1");
    const central = await Broker.getConfig(BrokerType.central);
    const module = await Broker.getConfig(BrokerType.module);
    const input = central["centreonBroker"]["input"].find(
      (output) => output.name === "central-broker-master-input"
    );
    const output = module["centreonBroker"]["output"].find(
      (output) => output.name === "central-module-master-output"
    );

    input["one_peer_retention_mode"] = "yes";
    input["host"] = "localhost";
    delete output["host"];
    output["one_peer_retention_mode"] = "yes";
    await Broker.writeConfig(BrokerType.central, central);
    await Broker.writeConfig(BrokerType.module, module);

    const broker = new Broker();
    const engine = new Engine();
    Engine.buildConfigs();

    const started1 = await broker.start();
    const started2 = await engine.start();
    const connected1 = await isBrokerAndEngineConnected();

    const stopped1 = await engine.stop();
    const cd1 = await broker.checkCoredump();
    const cd2 = await engine.checkCoredump();

    const running1 = await broker.isRunning(2, 5);

    const started3 = await engine.start();
    const connected2 = await isBrokerAndEngineConnected();

    const stopped2 = await engine.stop();
    const stopped3 = await broker.stop();

    const cd3 = await engine.checkCoredump();
    const cd4 = await broker.checkCoredump();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected1).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(cd1).toBeFalsy();
    expect(cd2).toBeFalsy();
    expect(running1).toBeTruthy();
    expect(started3).toBeTruthy();
    expect(connected2).toBeTruthy();
    expect(stopped2).toBeTruthy();
    expect(stopped3).toBeTruthy();
    expect(cd3).toBeFalsy();
    expect(cd4).toBeFalsy();
  }, 120000);

  it("BEOPR2: start/stop centreon broker/engine - broker first", async () => {
    console.log("BEOPR2");
    const central = await Broker.getConfig(BrokerType.central);
    const module = await Broker.getConfig(BrokerType.module);
    const input = central["centreonBroker"]["input"].find(
      (output) => output.name === "central-broker-master-input"
    );
    const output = module["centreonBroker"]["output"].find(
      (output) => output.name === "central-module-master-output"
    );

    input["one_peer_retention_mode"] = "yes";
    input["host"] = "localhost";
    delete output["host"];
    output["one_peer_retention_mode"] = "yes";
    await Broker.writeConfig(BrokerType.central, central);
    await Broker.writeConfig(BrokerType.module, module);

    const broker = new Broker();
    const engine = new Engine();
    Engine.buildConfigs();

    const started1 = await engine.start();
    const started2 = await broker.start();
    const connected1 = await isBrokerAndEngineConnected();

    const stopped1 = await broker.stop();
    const cd1 = await broker.checkCoredump();
    const cd2 = await engine.checkCoredump();

    const running1 = await engine.isRunning(2, 5);

    const started3 = await broker.start();
    const connected2 = await isBrokerAndEngineConnected();

    const stopped2 = await broker.stop();
    const stopped3 = await engine.stop();

    const cd3 = await broker.checkCoredump();
    const cd4 = await engine.checkCoredump();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected1).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(cd1).toBeFalsy();
    expect(cd2).toBeFalsy();
    expect(running1).toBeTruthy();
    expect(started3).toBeTruthy();
    expect(connected2).toBeTruthy();
    expect(stopped2).toBeTruthy();
    expect(stopped3).toBeTruthy();
    expect(cd3).toBeFalsy();
    expect(cd4).toBeFalsy();
  }, 100000);

  it("BEDB1: should handle database service stop and start", async () => {
    const broker = new Broker(2);
    const engine = new Engine();
    Engine.buildConfigs();

    const started1 = await broker.start();
    const started2 = await engine.start();

    const connected = await isBrokerAndEngineConnected();

    let cd1: boolean = false;
    let checkLog1: boolean = true;
    if (started1 && started2) {
      for (let i = 0; i < 5 && !cd1 && checkLog1; i++) {
        console.log(`BEDB1: ${i + 1}/5`);
        Broker.stopMysql();
        await sleep(1);
        cd1 = await broker.checkCoredump();
        Broker.startMysql();
        checkLog1 = await broker.checkCentralLogContains(
          ["[sql] [debug] conflict_manager: storage stream initialization"],
          30
        );
      }
    }

    const stopped1 = await broker.stop();
    const stopped2 = await engine.stop();
    const cd2: boolean = await broker.checkCoredump();
    const cd3: boolean = await engine.checkCoredump();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected).toBeTruthy();
    expect(cd1).toBeFalsy();
    expect(checkLog1).toBeTruthy();
    expect(cd2).toBeFalsy();
    expect(cd3).toBeFalsy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
  }, 100000);

  it("BEDB3: should handle database service stop and start", async () => {
    const broker = new Broker(2);
    const engine = new Engine();
    Engine.buildConfigs();

    const started1 = await broker.start();
    const started2 = await engine.start();

    const connected = await isBrokerAndEngineConnected();

    let cd1: boolean = false;
    let checkLog1: boolean = true;
    if (started1 && started2) {
      for (let i = 0; i < 5 && !cd1 && checkLog1; i++) {
        console.log(`BEDB3: ${i + 1}/5`);
        Broker.stopMysql();
        await sleep(10);
        cd1 = await broker.checkCoredump();
        Broker.startMysql();
        checkLog1 = await broker.checkCentralLogContains([
          "[sql] [debug] conflict_manager: storage stream initialization",
        ]);
        await sleep(10);
      }
    }

    const stopped1 = await broker.stop();
    const stopped2 = await engine.stop();
    const cd2: boolean = await broker.checkCoredump();
    const cd3: boolean = await engine.checkCoredump();

    Broker.cleanAllInstances();
    Engine.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected).toBeTruthy();
    expect(cd1).toBeFalsy();
    expect(checkLog1).toBeTruthy();
    expect(cd2).toBeFalsy();
    expect(cd3).toBeFalsy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
  }, 200000);
});
