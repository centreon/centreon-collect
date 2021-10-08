import sleep from "await-sleep";
import shell from "shelljs";
import { Broker, BrokerType } from "../core/broker";
import { Engine } from "../core/engine";
import { isBrokerAndEngineConnected } from "../core/brokerEngine";
import { fstat, readFileSync } from "fs";
import path = require("path");

shell.config.silent = true;

beforeEach(async () => {
  shell.exec("service mysqld start");
  await Broker.cleanAllInstances();
  await Engine.cleanAllInstances();

  Broker.resetConfig(BrokerType.central);
  Broker.clearLogs(BrokerType.central);
}, 30000);

afterEach(async () => {
  Broker.cleanAllInstances();
});

it("BDB1: should deny access when database name exists but is not the good one for sql output", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centralBrokerMasterSql = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-sql"
  );
  centralBrokerMasterSql["db_name"] = "centreon";
  await Broker.writeConfig(BrokerType.central, config);

  /* Loading of the two broker instances. */
  const broker = new Broker();
  for (let i = 0; i < 5; i++) {
    console.log(`BDB1 ${i + 1}/5`);
    const started = await broker.start();
    const checkLog1 = await broker.checkCentralLogContains([
      "Table 'centreon.instances' doesn't exist",
    ]);
    const stopped = await broker.stop();
    const cd = await broker.checkCoredump();

    Broker.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }
}, 120000);

it("BDB2: should deny access when database name exists but is not the good one for storage output", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centrealBrokerMasterPerfData = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-perfdata"
  );
  centrealBrokerMasterPerfData["db_name"] = "centreon";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker();
  for (let i = 0; i < 5; i++) {
    console.log(`BDB2 ${i + 1}/5`);
    const started = await broker.start();
    const checkLog1 = await broker.checkCentralLogContains([
      "Unable to connect to the database",
    ]);
    const stopped = await broker.stop();
    const cd = await broker.checkCoredump();

    /* Cleanup */
    Broker.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }
}, 100000);

it("BDB3: should deny access when database name does not exists for sql output", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centralBrokerMasterSql = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-sql"
  );
  centralBrokerMasterSql["db_name"] = "centreon1";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker();
  for (let i = 0; i < 5; i++) {
    console.log(`BDB3 ${i + 1}/5`);
    const started = await broker.start();
    let checkLog1 = await broker.checkCentralLogContains([
      "[core] [error] failover: global error: mysql_connection: error while starting connection",
    ]);
    const stopped = await broker.stop();
    const cd = await broker.checkCoredump();

    /* Cleanup */
    Broker.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }
}, 120000);

it("BDB4: should deny access when database name does not exist for storage output", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centrealBrokerMasterPerfData = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-perfdata"
  );
  centrealBrokerMasterPerfData["db_name"] = "centreon1";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker();
  for (let i = 0; i < 5; i++) {
    console.log(`BDB4 ${i + 1}/5`);
    const started = await broker.start();

    const checkLog1 = await broker.checkCentralLogContains([
      "Unable to connect to the database",
    ]);

    const stopped = await broker.stop();
    const cd = await broker.checkCoredump();

    /* Cleanup */
    Broker.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }
}, 120000);

it("BDB5: cbd does not crash if the storage db host is wrong", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centrealBrokerMasterPerfData = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-perfdata"
  );
  centrealBrokerMasterPerfData["db_host"] = "1.2.3.4";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker(1);
  for (let i = 0; i < 10; i++) {
    console.log(`BDB5 ${i + 1}/10`);
    const started = await broker.start();

    await sleep(2000);
    const checkLog1 = await broker.checkCentralLogContains([
      "[sql] [error] storage: rebuilder: Unable to connect to the database",
    ]);

    const stopped = await broker.stop();
    const cd = await broker.checkCoredump();

    /* Cleanup */
    Broker.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }
}, 200000);

it("BDB6: cbd does not crash if the sql db host is wrong", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centrealBrokerMasterPerfData = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-sql"
  );
  centrealBrokerMasterPerfData["db_host"] = "1.2.3.4";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker(1);
  for (let i = 0; i < 10; i++) {
    console.log(`BDB6 ${i + 1}/10`);
    const started = await broker.start();

    await sleep(2000);
    const checkLog1 = await broker.checkCentralLogContains([
      "Unable to initialize the storage connection to the database",
    ]);

    const stopped = await broker.stop();
    const cd = await broker.checkCoredump();

    /* Cleanup */
    Broker.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(checkLog1).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(cd).toBeFalsy();
  }
}, 200000);

it("BDB7: should deny access when database user password is wrong for perfdata/sql", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centralBrokerMasterSql = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-sql"
  );
  centralBrokerMasterSql["db_password"] = "centreon1";
  const centralBrokerMasterPerfdata = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-perfdata"
  );
  centralBrokerMasterPerfdata["db_password"] = "centreon1";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker();
  const started = await broker.start();

  const checkLog = await broker.checkCentralLogContains([
    "[sql] [error] mysql_connection: error while starting connection",
  ]);

  const stopped = await broker.stop();
  const cd = await broker.checkCoredump();

  /* Cleanup */
  Broker.cleanAllInstances();

  expect(started).toBeTruthy();
  expect(checkLog).toBeTruthy();
  expect(stopped).toBeTruthy();
  expect(cd).toBeFalsy();
}, 100000);

it("BDB8: should deny access when database user password is wrong for perfdata", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centralBrokerMasterPerfdata = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-perfdata"
  );
  centralBrokerMasterPerfdata["db_password"] = "centreon1";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker();
  const started = await broker.start();

  const checkLog = await broker.checkCentralLogContains(
    ["[sql] [error] mysql_connection: error while starting connection"],
    60
  );

  const stopped = await broker.stop();
  const cd = await broker.checkCoredump();

  /* Cleanup */
  Broker.cleanAllInstances();

  expect(started).toBeTruthy();
  expect(checkLog).toBeTruthy();
  expect(stopped).toBeTruthy();
  expect(cd).toBeFalsy();
}, 65000);

it("BDB9: should deny access when database user password is wrong for sql", async () => {
  const config = await Broker.getConfig(BrokerType.central);
  const centralBrokerMasterSql = config["centreonBroker"]["output"].find(
    (output) => output.name === "central-broker-master-sql"
  );
  centralBrokerMasterSql["db_password"] = "centreon1";
  await Broker.writeConfig(BrokerType.central, config);

  const broker = new Broker();
  const started = await broker.start();

  const checkLog = await broker.checkCentralLogContains(
    ["[sql] [error] mysql_connection: error while starting connection"],
    60
  );

  const stopped = await broker.stop();
  const cd = await broker.checkCoredump();

  /* Cleanup */
  Broker.cleanAllInstances();

  expect(started).toBeTruthy();
  expect(checkLog).toBeTruthy();
  expect(stopped).toBeTruthy();
  expect(cd).toBeFalsy();
}, 65000);

it("BDB10: should connect when database user password is good for sql/perfdata", async () => {
  const broker = new Broker();
  const started = await broker.start();

  const checkLog = await broker.checkCentralLogContains([
    "sql stream initialization",
    "storage stream initialization",
  ]);

  const stopped = await broker.stop();
  const cd = await broker.checkCoredump();

  /* Cleanup */
  Broker.cleanAllInstances();

  expect(started).toBeTruthy();
  expect(checkLog).toBeTruthy();
  expect(stopped).toBeTruthy();
  expect(cd).toBeFalsy();
}, 65000);

it("BEDB2: start broker/engine and then start MySQL", async () => {
  Broker.stopMysql();
  const broker = new Broker();
  const engine = new Engine();
  Engine.buildConfigs();

  const started1 = await broker.start();
  const started2 = await engine.start();

  const checkLog1 = await broker.checkCentralLogContains([
    "Unable to connect to the database",
  ]);

  Broker.startMysql();

  let statsOK: boolean = false;
  let limit = Date.now() + 30000;
  while (Date.now() < limit) {
    let b = readFileSync(
      "/var/lib/centreon-broker/central-broker-master-stats.json"
    );
    let stats = JSON.parse(b.toString());
    if (stats["mysql manager"]["waiting tasks in connection 0"] !== undefined) {
      statsOK = true;
      break;
    }
    await sleep(200);
  }

  const stopped1 = await broker.stop();
  const stopped2 = await engine.stop();
  const cd = await broker.checkCoredump();

  /* Cleanup */
  Broker.cleanAllInstances();
  Engine.cleanAllInstances();

  expect(started1).toBeTruthy();
  expect(started2).toBeTruthy();
  expect(checkLog1).toBeTruthy();
  expect(stopped1).toBeTruthy();
  expect(stopped2).toBeTruthy();
  expect(cd).toBeFalsy();
  expect(statsOK).toBeTruthy();
}, 80000);

it("BDBM1: start broker/engine and then start MySQL", async () => {
  Broker.startMysql();
  for (let count of [4, 6]) {
    const config = await Broker.getConfig(BrokerType.central);
    const centralBrokerMasterSql = config["centreonBroker"]["output"].find(
      (output) => output.name === "central-broker-master-sql"
    );
    centralBrokerMasterSql["connections_count"] = `${count}`;

    const centralBrokerMasterStorage = config["centreonBroker"]["output"].find(
      (output) => output.name === "central-broker-master-perfdata"
    );
    centralBrokerMasterStorage["connections_count"] = `${count}`;
    await Broker.writeConfig(BrokerType.central, config);

    const broker = new Broker();

    const started1 = await broker.start();

    let limit = Date.now() + 30000;
    let conCount = 0;
    while (Date.now() < limit) {
      let b = readFileSync(
        "/var/lib/centreon-broker/central-broker-master-stats.json"
      );
      let stats = await JSON.parse(b.toString());
      let mm: JSON = stats["mysql manager"];
      for (let key in mm) {
        if (key.includes("waiting tasks")) conCount++;
      }
      if (conCount > 0) break;
      await sleep(200);
    }

    const stopped1 = await broker.stop();
    const cd = await broker.checkCoredump();

    /* Cleanup */
    Broker.cleanAllInstances();

    expect(started1).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(cd).toBeFalsy();
    expect(conCount).toEqual(count);
  }
}, 80000);

