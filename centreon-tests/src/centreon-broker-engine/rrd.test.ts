import shell from "shelljs";
import { Broker, BrokerType } from "../core/broker";
import { Engine } from "../core/engine";
import { isBrokerAndEngineConnected } from "../core/brokerEngine";
import { readdirSync, statSync } from "fs";
import { readdir } from "fs/promises";
import sleep from "await-sleep";
import { createConnection } from "promise-mysql";
import { exit } from "process";

shell.config.silent = true;

describe("engine reloads with new hosts and hostgroups configurations", () => {
  beforeEach(async () => {
    await Engine.cleanAllInstances();
    await Broker.cleanAllInstances();
    let s = statSync("/var/lib/centreon/metrics");
    if (s.uid != Broker.CENTREON_BROKER_UID) {
      console.error("The folder '/var/lib/centreon/metrics'");
      exit(1);
    }
    s = statSync("/var/lib/centreon/status");
    if (s.uid != Broker.CENTREON_BROKER_UID) {
      console.error("The folder '/var/lib/centreon/status'");
      exit(1);
    }
    expect(s.uid).toEqual(Broker.CENTREON_BROKER_UID);
    Broker.startMysql();
    Broker.clearLogs(BrokerType.central);
    Broker.clearRetention(BrokerType.central);
    Broker.clearRetention(BrokerType.rrd);
    Broker.clearRetention(BrokerType.module);
    Broker.resetConfig(BrokerType.central);

    Engine.clearLogs();

    if (Broker.isInstancesRunning() || Engine.isInstancesRunning()) {
      console.log("program could not stop cbd or centengine");
      process.exit(1);
    }
  }, 30000);

  /* RRD metric deletion */
  it("BRRDDM1: RRD metric deletion", async () => {
    const central = await Broker.getConfig(BrokerType.central);
    var loggers = central["centreonBroker"]["log"]["loggers"];
    loggers["perfdata"] = "debug";
    await Broker.writeConfig(BrokerType.central, central);

    const rrd = await Broker.getConfig(BrokerType.rrd);
    loggers = rrd["centreonBroker"]["log"]["loggers"];
    loggers["rrd"] = "debug";
    await Broker.writeConfig(BrokerType.rrd, rrd);

    const broker = new Broker(2);
    const engine = new Engine();
    await Engine.buildConfigs();
    const started1 = await broker.start();
    const started2 = await engine.start();

    const connected1 = await isBrokerAndEngineConnected();

    const db = await createConnection({
      database: "centreon_storage",
      host: "localhost",
      user: "centreon",
      password: "centreon",
    });
    /* Let's get all the metric_id from the database */
    let rows = await db.query(
      `SELECT metric_id FROM metrics ORDER BY metric_id`
    );

    /* We need to get a metric : ls /var/lib/centreon/metrics/*.rrd | head*/
    let files = readdirSync("/var/lib/centreon/metrics");
    let metric: string;
    for (let m in rows) {
      if (files.some((v) => v === m + ".rrd")) {
        metric = m;
        break;
      }
    }
    console.log(`Trying to delete file '${metric}.rrd'`);
    console.log(`Metric '${metric}' to remove`);

    await db.query(
      `UPDATE index_data i LEFT JOIN metrics m ON i.id=m.index_id SET i.to_delete=1 WHERE m.metric_id=${metric}`
    );

    await broker.reload();

    let dbResult: boolean = false;
    let fileResult: boolean = false;
    let limit = Date.now() + 6 * 60000;
    while (Date.now() < limit && !dbResult) {
      let rows = await db.query(
        `SELECT metric_id FROM metrics WHERE metric_id=${metric}`
      );

      if (rows.length === 0) {
        dbResult = true;
      } else await sleep(5000);
    }
    let metricfile = metric + ".rrd";
    while (Date.now() < limit && !fileResult) {
      files = await readdir("/var/lib/centreon/metrics");
      fileResult = files.every((v) => v !== metricfile);
      if (!fileResult) await sleep(5000);
      else console.log(`File '${metricfile}' deleted`);
    }
    if (Date.now() >= limit) fileResult = false;

    console.log("Stopping engine");
    const stopped1: boolean = await engine.stop();
    console.log("Stopping broker");
    const stopped2: boolean = await broker.stop();
    db.end();

    await Broker.cleanAllInstances();
    await Engine.cleanAllInstances();

    console.log("Checking results");
    expect(dbResult).toBeTruthy();
    expect(fileResult).toBeTruthy();
    expect(started1).toBeTruthy();
    expect(started2).toBeTruthy();
    expect(connected1).toBeTruthy();
    expect(stopped1).toBeTruthy();
    expect(stopped2).toBeTruthy();
  }, 720000);
});
