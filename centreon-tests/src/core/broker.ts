

import shell from 'shelljs'
import psList from 'ps-list'
import fs from 'fs/promises'
import { once } from 'events'
import { ChildProcess } from 'child_process'
import sleep from 'await-sleep';
import path from 'path';
import { strict as assert } from 'assert';


export class Broker {
    private instanceCount: number
    private process: ChildProcess
    private rddProcess: ChildProcess
    private config: JSON;

    static CENTREON_BROKER_UID = parseInt(shell.exec('id -u centreon-broker'))
    static CENTREON_BROKER_LOGS_PATH = `/var/log/centreon-broker/central-broker-master.log`
    static CENTRON_BROKER_CONFIG_PATH = `/etc/centreon-broker/central-broker.json`

    constructor(count: number = 2) {
      assert(count == 1 || count == 2)
      this.instanceCount = count
    }


    /**
     * this function will start a new centreon broker and rdd process
     * upon completition
     *
     * @returns Promise<Boolean> true if correctly started, else false
     */
    async start(): Promise<Boolean> {
      this.process = shell.exec(`/usr/sbin/cbd ${Broker.CENTRON_BROKER_CONFIG_PATH}`, {async: true, uid: Broker.CENTREON_BROKER_UID})
      if (this.instanceCount == 2)
        this.rddProcess = shell.exec(`/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`, {async: true, uid: Broker.CENTREON_BROKER_UID})

      const isRunning = await this.isRunning(true, 20)
      return isRunning;
    }


    /**
     * will stop current cbd broker if already running
     *
     * @returns Promise<Boolean> true if correctly stoped, else false
     */
    async stop(): Promise<Boolean> {
        if(await this.isRunning(true, 5)) {
            this.process.kill()
            if (this.instanceCount == 2)
              this.rddProcess.kill()

            const isRunning = await this.isRunning(false)
            return !isRunning;
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
    async isRunning(expected: boolean = true, seconds: number = 15) : Promise<boolean> {
        let centreonBrokerProcess;
        let centreonRddProcess;

        for(let i = 0; i < seconds * 2; ++i) {

          const processList = await psList();

          centreonBrokerProcess = processList.find((process) => process.pid == this.process.pid);

          if (this.instanceCount == 2)
            centreonRddProcess = processList.find((process) => process.pid == this.rddProcess.pid);
          else
            centreonRddProcess = expected

          if (centreonBrokerProcess && centreonRddProcess && expected)
            return true;

          if (expected == false && !centreonRddProcess && !centreonBrokerProcess)
            return false;

          await sleep(500)
        }

        return !expected;
    }

    static async checkCoredump(): Promise<boolean> {
      const cdList = await shell.exec('/usr/bin/coredumpctl').stdout
      let retval = cdList.includes('cbd')
      return retval
    }

    /**
     * this retrive the current centreon config
     *
     * @returns Promise<JSON> config json object
     */
    static async getConfig(): Promise<JSON> {
      return JSON.parse((await fs.readFile('/etc/centreon-broker/central-broker.json')).toString());
    }


    /**
     * write json config to centreon default config file location
     * @param  {JSON} config object representing broker configuration
     */
    static async writeConfig(config: JSON) {
        await fs.writeFile('/etc/centreon-broker/central-broker.json', JSON.stringify(config, null, '\t'))
    }


    /**
     * this reset the default configuration for broker</Boolean>
     * very useful for resetting after doing some tests
     */
    static resetConfig() {
      return shell.cp(path.join(__dirname, '../config/centreon-broker.json'), Broker.CENTRON_BROKER_CONFIG_PATH)
    }


    /**
     *  this function is usefu for checking that a log file contain some string
     * @param  {Array<string>} strings list of string to check, every string in this array must be found in logs file
     * @param  {number} seconds=15 number of second to wait before returning
     * @returns {Promise<Boolean>} true if found, else false
     */
    static async checkLogFileContains(strings: Array<string>, seconds = 15): Promise<Boolean> {

      for (let i = 0; i < seconds * 10; ++i) {
        const logs = await Broker.getLogs()

        if (logs.includes(strings[0])) {
          return true;
        }

        await sleep(100)
      }

     throw Error(`log file ${Broker.CENTREON_BROKER_LOGS_PATH} does not contain expected strings ${strings.toString()}`)
    }

    static async getLogs(): Promise<String> {
      return ( await fs.readFile(Broker.CENTREON_BROKER_LOGS_PATH)).toString()
    }

    static clearLogs(): void {
      shell.rm(Broker.CENTREON_BROKER_LOGS_PATH)
    }
}
