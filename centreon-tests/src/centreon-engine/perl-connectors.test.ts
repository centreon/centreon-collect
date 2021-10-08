import sleep from "await-sleep";
import shell from "shelljs";
import { Engine } from "../core/engine";
import { Broker, BrokerType } from "../core/broker";

shell.config.silent = true;

describe("start and stop engine", () => {
  beforeEach(() => {
    Broker.resetConfig(BrokerType.central);
    Broker.resetConfig(BrokerType.module);
    Broker.resetConfig(BrokerType.rrd);
    Broker.cleanAllInstances();

    Engine.cleanAllInstances();
    Engine.clearLogs();
  }, 30000);

  it("EPC1: Check with perl connector", async () => {
    const engine = new Engine();
    // We activate 'commands debug'
    Engine.buildConfigs(50, 40, { debugLevel: 256 });
    const started = await engine.start();
    const checkDebug = await engine.checkDebugFileContains(0, [
      "connector::run: connector='Perl Connector'",
      "connector::data_is_available",
    ]);
    const stopped = await engine.stop();
    const cd = await engine.checkCoredump();

    Engine.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(checkDebug).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }, 30000);
});
