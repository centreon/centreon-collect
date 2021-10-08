import shell from "shelljs";
import psList from "ps-list";
import {
  chownSync,
  unlinkSync,
  existsSync,
  createReadStream,
  rmSync,
  writeFileSync,
  writeSync,
  readdirSync,
  copyFileSync,
} from "fs";
import fs from "fs/promises";
import { ChildProcess } from "child_process";
import sleep from "await-sleep";
import path from "path";
import { strict as assert } from "assert";
import { SIGHUP, SIGTERM } from "constants";
import readline from "readline";

export enum BrokerType {
  central = 0,
  rrd = 1,
  module = 2,
}
export class Broker {
  private instanceCount: number;
  private process: ChildProcess;
  private rrdProcess: ChildProcess;

  static CENTREON_BROKER_UID = parseInt(shell.exec("id -u centreon-broker"));
  static CENTREON_BROKER_GID = parseInt(shell.exec("id -g centreon-broker"));
  static CENTREON_ENGINE_UID = parseInt(shell.exec("id -u centreon-engine"));
  static CENTREON_ENGINE_GID = parseInt(shell.exec("id -g centreon-engine"));
  static CENTREON_BROKER_CENTRAL_LOGS_PATH = `/var/log/centreon-broker/central-broker-master.log`;
  static CENTREON_BROKER_RRD_LOGS_PATH = `/var/log/centreon-broker/central-rrd-master.log`;
  static CENTREON_BROKER_MODULE_LOGS_PATH = `/var/log/centreon-broker/central-module-master.log`;
  static CENTREON_BROKER_CONFIG_PATH: { [index: number]: string } = [
    "/etc/centreon-broker/central-broker.json",
    "/etc/centreon-broker/central-rrd.json",
    "/etc/centreon-broker/central-module.json",
  ];
  static lastMatchingLog: { [index: number]: number } = [
    Math.floor(Date.now() / 1000),
    Math.floor(Date.now() / 1000),
    Math.floor(Date.now() / 1000),
  ];

  constructor(count: number = 2) {
    assert(count == 1 || count == 2);
    this.instanceCount = count;
    let d = Math.floor(Date.now() / 1000);
    Broker.lastMatchingLog = [d, d, d];
  }

  /**
   * this function will start a new centreon broker and rdd process
   * upon completition
   *
   * @returns Promise<Boolean> true if correctly started, else false
   */
  async start(): Promise<boolean> {
    if (!(await this.isRunning(1))) {
      this.process = shell.exec(
        `/usr/sbin/cbd ${
          Broker.CENTREON_BROKER_CONFIG_PATH[BrokerType.central]
        }`,
        {
          async: true,
          uid: Broker.CENTREON_BROKER_UID,
          gid: Broker.CENTREON_BROKER_GID,
        }
      );
      if (this.instanceCount == 2)
        this.rrdProcess = shell.exec(
          `/usr/sbin/cbd ${Broker.CENTREON_BROKER_CONFIG_PATH[BrokerType.rrd]}`,
          {
            async: true,
            uid: Broker.CENTREON_BROKER_UID,
            gid: Broker.CENTREON_BROKER_GID,
          }
        );

      return await this.isRunning(5, 2);
    }
    return true;
  }

  /**
   * will stop current cbd broker if already running
   *
   * @returns Promise<Boolean> true if correctly stopped, else false
   */
  async stop(): Promise<boolean> {
    if (!(await this.isStopped(1))) {
      this.process.kill(SIGTERM);

      if (this.instanceCount == 2) this.rrdProcess.kill(SIGTERM);

      return this.isStopped(60);
    }
    return true;
  }

  /**
   * check the list of all running processes, and return true if they are *all* running.
   *
   * @param  {boolean=true} expected the expected value, true or false
   * @param  {number=15} waitTime number of seconds to wait for process to show in processlist
   * @param  {number=0} flapTime duration to check that everything is working
   * @returns Promise<Boolean>
   */
  async isRunning(
    waitTime: number = 15,
    flapTime: number = 0
  ): Promise<boolean> {
    if (!this.process || (this.instanceCount == 2 && !this.rrdProcess))
      return false;
    let centralProcess: psList.ProcessDescriptor;
    let rrdProcess: psList.ProcessDescriptor;

    let now = Date.now();
    let limit = now + waitTime * 1000;
    while (now < limit) {
      const processList = await psList();
      if (!centralProcess)
        centralProcess = processList.find(
          (process) => process.pid == this.process.pid
        );
      if (!rrdProcess && this.instanceCount == 2)
        rrdProcess = processList.find(
          (process) => process.pid == this.rrdProcess.pid
        );

      if (centralProcess && (this.instanceCount == 1 || rrdProcess)) break;
      await sleep(200);
      now = Date.now();
    }
    if (!centralProcess || (this.instanceCount == 2 && !rrdProcess))
      return false;

    now = Date.now();
    limit = now + flapTime * 1000;
    while (now < limit) {
      await sleep(500);
      const processList = await psList();
      centralProcess = processList.find(
        (process) => process.pid == this.process.pid
      );
      if (this.instanceCount == 2)
        rrdProcess = processList.find(
          (process) => process.pid == this.rrdProcess.pid
        );
      if (!centralProcess || (this.instanceCount == 2 && !rrdProcess))
        return false;
      now = Date.now();
    }

    return true;
  }

  /**
   * this function will check the list of all process running in current os
   * to check that the current instance of broker is correctly running or not
   *
   * @param  {boolean=true} expected the expected value, true or false
   * @param  {number=15} seconds number of seconds to wait for process to show in processlist
   * @returns Promise<Boolean>
   */
  async isStopped(seconds: number = 15): Promise<boolean> {
    let centralProcess: psList.ProcessDescriptor;
    let rrdProcess: psList.ProcessDescriptor;

    let now = Date.now();
    let limit = now + seconds * 1000;
    while (now < limit) {
      const processList = await psList();
      centralProcess = processList.find(
        (process) => process.pid == this.process.pid
      );
      if (this.instanceCount == 2)
        rrdProcess = processList.find(
          (process) => process.pid == this.rrdProcess.pid
        );

      if (!centralProcess && (this.instanceCount == 1 || !rrdProcess))
        return true;
      await sleep(500);
      now = Date.now();
    }
    return false;
  }

  async reload() {
    if (await this.isRunning(5)) {
      if (this.instanceCount == 2) this.rrdProcess.kill(SIGHUP);
      this.process.kill(SIGHUP);
    }
  }

  async checkCoredump(): Promise<boolean> {
    let retval: string;
    const cdList = shell.exec("ps ax").stdout.split("\n");
    retval = cdList.find((line) =>
      line.includes("/usr/lib/systemd/systemd-coredump")
    );

    if (!retval) {
      const cdList = await shell
        .exec("/usr/bin/coredumpctl")
        .stdout.split("\n");
      if (this.instanceCount == 1)
        retval = cdList.find(
          (line) => line.includes("cbd") && line.includes(this.process.pid + "")
        );
      else
        retval = cdList.find(
          (line) =>
            line.includes("cbd") &&
            (line.includes(this.process.pid + "") ||
              line.includes(this.rrdProcess.pid + ""))
        );
    }
    if (retval) return true;
    else return false;
  }

  /**
   * this retrive the current centreon config
   *
   * @returns Promise<JSON> config json object
   */
  static async getConfig(type: BrokerType): Promise<JSON> {
    return JSON.parse(
      (await fs.readFile(this.CENTREON_BROKER_CONFIG_PATH[type])).toString()
    );
  }

  /**
   * write json config to centreon default config file location
   * @param  {JSON} config object representing broker configuration
   */
  static async writeConfig(type: BrokerType, config: JSON) {
    await fs.writeFile(
      this.CENTREON_BROKER_CONFIG_PATH[type],
      JSON.stringify(config, null, "\t")
    );
  }

  /**
   * this reset the default configuration for broker</Boolean>
   * very useful for resetting after doing some tests
   */
  static resetConfig(type: BrokerType) {
    let conf: string;
    switch (type) {
      case BrokerType.central:
        conf = "../config/central-broker.json";
        break;
      case BrokerType.module:
        conf = "../config/central-module.json";
        break;
      case BrokerType.rrd:
        conf = "../config/central-rrd.json";
        break;
    }
    copyFileSync(
      path.join(__dirname, conf),
      Broker.CENTREON_BROKER_CONFIG_PATH[type]
    );
  }

  /**
   *  this function is useful for checking that a log file contain some string
   * @param  {Array<string>} strings list of string to check, every string in this array must be found in logs file
   * @param  {number} seconds=15 number of second to wait before returning
   * @returns {Promise<Boolean>} true if found, else false
   */
  async checkLogFileContains(
    b: BrokerType,
    strings: string[],
    seconds: number
  ): Promise<boolean> {
    let logname: string;
    switch (b) {
      case BrokerType.central:
        logname = Broker.CENTREON_BROKER_CENTRAL_LOGS_PATH;
        break;
      case BrokerType.module:
        logname = Broker.CENTREON_BROKER_MODULE_LOGS_PATH;
        break;
      case BrokerType.rrd:
        logname = Broker.CENTREON_BROKER_RRD_LOGS_PATH;
        break;
    }

    let from = Broker.lastMatchingLog[b];

    /* 3 possible values:
     * 0 => failed
     * 1 => succeed
     * 2 => start again (the file reached its end without success).
     */
    let retval: Promise<number>;

    do {
      let p = new Promise((resolve, reject) => {
        const rl = readline.createInterface({
          input: createReadStream(logname),
          terminal: false,
        });
        rl.on("line", (line) => {
          let d = line.substring(1, 24);
          let dd = Date.parse(d) / 1000;
          if (dd >= from) {
            let idx = strings.findIndex((s) => line.includes(s));
            if (idx >= 0) {
              Broker.lastMatchingLog[b] = dd;
              strings.splice(idx, 1);
              if (strings.length === 0) {
                resolve(true);
                rl.close();
                return;
              }
            }
            if (dd - from > seconds) {
              reject(
                `Timeout: From timestamp ${from} to timestamp ${dd} ; cannot find strings <<${strings.join(
                  ", "
                )}>> in centengine.log`
              );
              rl.close();
              return;
            }
          }
        });
        rl.on("close", () => {
          reject("File closed");
        });
      });

      retval = p
        .then((value: boolean) => {
          if (!value) {
            console.log(
              `Cannot find strings <<${strings.join(", ")}>> in broker logs`
            );
            return 0;
          } else return 1;
        })
        .catch((err) => {
          if (err == "File closed") return 2;
          else {
            console.log(
              `Cannot find strings <<${strings.join(", ")}>> in broker logs`
            );
            return 0;
          }
        });
    } while ((await retval) == 2);
    return (await retval) > 0;
  }

  async checkCentralLogContains(
    strings: string[],
    seconds: number = 15
  ): Promise<boolean> {
    return await this.checkLogFileContains(
      BrokerType.central,
      strings,
      seconds
    );
  }

  async checkRrdLogContains(
    strings: string[],
    seconds: number = 15
  ): Promise<boolean> {
    return await this.checkLogFileContains(BrokerType.rrd, strings, seconds);
  }

  async checkModuleLogContains(
    strings: string[],
    seconds: number = 15
  ): Promise<boolean> {
    return await this.checkLogFileContains(BrokerType.module, strings, seconds);
  }

  static clearLogs(type: BrokerType): void {
    let logname: string;
    let uid: number;
    let gid: number;
    let d = Math.floor(Date.now() / 1000);
    Broker.lastMatchingLog[type] = d;
    switch (type) {
      case BrokerType.central:
        logname = Broker.CENTREON_BROKER_CENTRAL_LOGS_PATH;
        uid = Broker.CENTREON_BROKER_UID;
        gid = Broker.CENTREON_BROKER_GID;
        break;
      case BrokerType.module:
        logname = Broker.CENTREON_BROKER_MODULE_LOGS_PATH;
        uid = Broker.CENTREON_ENGINE_UID;
        gid = Broker.CENTREON_ENGINE_GID;
        break;
      case BrokerType.rrd:
        logname = Broker.CENTREON_BROKER_RRD_LOGS_PATH;
        uid = Broker.CENTREON_BROKER_UID;
        gid = Broker.CENTREON_BROKER_GID;
        break;
    }
    if (existsSync(logname))
      rmSync(logname);

    writeFileSync(logname, "");
    chownSync(logname, uid, gid);
  }

  static clearRetention(type: BrokerType): void {
    let path: string;
    let regex: RegExp;
    switch (type) {
      case BrokerType.central:
        path = "/var/lib/centreon-broker/";
        regex = /central-broker-master\..*/;
        break;
      case BrokerType.module:
        path = "/var/lib/centreon-engine/";
        regex = /central-module-master\..*/;
        break;
      case BrokerType.rrd:
        path = "/var/lib/centreon-broker/";
        regex = /central-rrd-master\..*/;
        break;
    }
    readdirSync(path)
      .filter((f) => regex.test(f))
      .map((f) => unlinkSync(path + f));
  }

  static clearLogsCentralModule(): void {
    if (existsSync(Broker.CENTREON_BROKER_CENTRAL_LOGS_PATH))
      rmSync(Broker.CENTREON_BROKER_CENTRAL_LOGS_PATH);
    if (existsSync(Broker.CENTREON_BROKER_RRD_LOGS_PATH))
      rmSync(Broker.CENTREON_BROKER_RRD_LOGS_PATH);
    if (existsSync(Broker.CENTREON_BROKER_MODULE_LOGS_PATH))
      rmSync(Broker.CENTREON_BROKER_MODULE_LOGS_PATH);
  }

  static startMysql(): void {
    if (this.isMySqlStarted(1)) return;
    shell.exec("systemctl start mysqld");
  }

  static stopMysql(): void {
    if (this.isMySqlStopped(1)) return;
    shell.exec("systemctl stop mysqld");
  }

  static isMySqlStarted(waitTime: number = 20): boolean {
    let now = Date.now();
    const limit = now + waitTime * 1000;

    while (now < limit) {
      const cdList = shell.exec("systemctl status mysql").stdout.split("\n");
      let retval = cdList.find((line) => line.includes("active (running)"));
      if (retval) return true;
      now = Date.now();
    }
    return false;
  }
  static isMySqlStopped(waitTime: number = 20): boolean {
    let now = Date.now();
    const limit = now + waitTime * 1000;

    while (now < limit) {
      const cdList = shell.exec("systemctl status mysql").stdout.split("\n");
      let retval = cdList.find((line) => line.includes("inactive"));
      if (retval) return true;
      now = Date.now();
    }
    return false;
  }

  /**
   *  this function checks if instances of cbd are actually running
   * @param  {void}
   * @returns {Promise<Boolean>} true if found, else false
   */
  static isServiceRunning(): boolean {
    /* checks if we have an active systemctl status */
    const cdList = shell.exec("systemctl status cbd").stdout.split("\n");
    if (cdList.find((line) => line.includes("running"))) return true;
    else return false;
  }

  /**
   *  this function checks if instances of cbd are actually running
   * @param  {void}
   * @returns {Boolean} true if found, else false
   */
  static isInstancesRunning(): boolean {
    let instances = shell
      .exec('ps ax |grep -v grep | grep "/sbin/cbd"')
      .stdout.split("\n");

    instances = instances.filter(String);

    if (instances != undefined && instances.length) return true;
    else return false;
  }

  /**
   *  this function close instances of cbd that are actually running
   * @param  {void}
   * @returns {void} true if found, else false
   */
  static async closeInstances(): Promise<void> {
    const processList = await psList();
    processList.forEach((process) => {
      if (process.name == "cbd") shell.exec(`kill -9 ${process.pid}`);
    });
  }

  static async cleanAllInstances(): Promise<void> {
    /* close cbd if running */
    if (Broker.isServiceRunning()) shell.exec("systemctl stop cbd");

    /* closes instances of cbd if running */
    if (Broker.isInstancesRunning()) {
      await Broker.closeInstances();
    }
  }
}
