import sleep from "await-sleep";
import shell from "shelljs";
import { Broker, BrokerType } from "../core/broker";
import { Engine } from "../core/engine";

shell.config.silent = true;

describe("broker testing", () => {
  beforeEach(() => {
    Broker.cleanAllInstances();
    Engine.cleanAllInstances();
    Broker.clearLogs(BrokerType.central);
    Broker.resetConfig(BrokerType.central);
    Broker.resetConfig(BrokerType.rrd);
  });

  /**
   * The two instances of broker are started. Then we check they are correctly started.
   * The two instances are stopped. Then we check they are correctly stopped.
   * And we check no coredump has been produced.
   */
  it("BSS1: start/stop centreon broker => no coredump", async () => {
    const broker = new Broker();

    const isStarted = await broker.start();
    let isStopped: boolean = false;
    if (isStarted) {
      isStopped = await broker.stop();
    }
    let cd = await broker.checkCoredump();
    Broker.cleanAllInstances();
    expect(isStarted).toBeTruthy();
    expect(isStopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }, 60000);

  /**
   * One instance of broker is started. Then we check it is correctly started.
   * It is then stopped. And we check it is correctly stopped.
   * And we check no coredump has been produced.
   */
  it("BSS3: start/stop centreon broker => no coredump", async () => {
    const broker = new Broker(1);

    const isStarted = await broker.start();
    let isStopped: boolean = false;
    if (isStarted) {
      isStopped = await broker.stop();
    }
    let cd: boolean = await broker.checkCoredump();
    Broker.cleanAllInstances();
    expect(isStarted).toBeTruthy();
    expect(isStopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }, 60000);

  it("start/stop centreon broker with reversed connection on TCP acceptor but only this instance => no deadlock", async () => {
    /* Let's get the configuration, we remove the host to connect since we wan't the other peer
     * to establish the connection. We also set the one peer retention mode (just for the configuration
     * to be correct, not needed for the test). */
    const config = await Broker.getConfig(BrokerType.central);
    const centralBrokerMasterRRD = config["centreonBroker"]["output"].find(
      (output) => output.name === "centreon-broker-master-rrd"
    );
    delete centralBrokerMasterRRD.host;
    centralBrokerMasterRRD["one_peer_retention_mode"] = "yes";
    await Broker.writeConfig(BrokerType.central, config);

    /* Preparing one instance of broker */
    const broker = new Broker(1);

    /* Starting it */
    const isStarted = await broker.start();
    let isStopped = false;
    if (isStarted) {
      isStopped = await broker.stop();
    }
    Broker.cleanAllInstances();
    expect(isStarted).toBeTruthy();
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy();
  }, 60000);

  it("BSS2: repeat 10 times start/stop broker with .3sec interval => no coredump", async () => {
    /* Preparing the two instances of broker */
    const broker = new Broker(1);
    for (let i = 0; i < 10; ++i) {
      const isStarted = await broker.start();
      await sleep(300);
      const isStopped = await broker.stop();
      const cd = await broker.checkCoredump();

      /* Little cleanup */
      Broker.cleanAllInstances();

      expect(isStarted).toBeTruthy();
      expect(isStopped).toBeTruthy();
      expect(cd).toBeFalsy();
    }
  }, 240000);

  it("BSS3: repeat 10 times start/stop broker with 1sec interval => no coredump", async () => {
    /* Preparing the two instances of broker */
    const broker = new Broker(1);
    for (let i = 0; i < 10; ++i) {
      const isStarted = await broker.start();
      await sleep(1000);
      const isStopped = await broker.stop();

      /* Little cleanup */
      Broker.cleanAllInstances();

      expect(isStarted).toBeTruthy();
      expect(isStopped).toBeTruthy();
      expect(await broker.checkCoredump()).toBeFalsy();
    }
  }, 300000);
});
