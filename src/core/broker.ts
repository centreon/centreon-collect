

import shell from 'shelljs'
import psList from 'ps-list'
import fs from 'fs/promises'
import { once } from 'events'
import { ChildProcess } from 'child_process'
import sleep from 'await-sleep';
import path from 'path';

export class Broker {
    private process: ChildProcess
    private rddProcess: ChildProcess
    private config: JSON;

    static CENTREON_BROKER_UID = parseInt(shell.exec('id -u centreon-broker'))
    static CENTREON_BROKER_LOGS_PATH = `/var/log/centreon-broker/central-broker-master.log`
    static CENTRON_BROKER_CONFIG_PATH = `/etc/centreon-broker/central-broker.json`
    
    constructor() {
    }


    /**
     * this function will start a new centreon broker and rdd process
     * upon completition 
     */
    async start(): Promise<Boolean> {
        this.process = shell.exec(`/usr/sbin/cbd ${Broker.CENTRON_BROKER_CONFIG_PATH}`, {async: true, uid: Broker.CENTREON_BROKER_UID})
        this.rddProcess = shell.exec(`/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`, {async: true, uid: Broker.CENTREON_BROKER_UID})

        const isRunning = await this.isRunning(true, 20)
        return isRunning;
    }

    async stop() {
        if(await this.isRunning(true, 5)) {
            this.process.kill()
            this.rddProcess.kill()

            const isRunning = await this.isRunning(false)
            return !isRunning;
        }

        return true;
    }


    async  isRunning(expected: boolean = true, seconds: number = 15) : Promise<boolean> {
        let centreonBrokerProcess;
        let centreonRddProcess;

        for(let i = 0; i < seconds * 2; ++i) {
    
          const processList = await psList();
    
          centreonBrokerProcess = processList.find((process) => process.pid == this.process.pid);
          centreonRddProcess = processList.find((process) => process.pid == this.rddProcess.pid);
          
          if(centreonBrokerProcess  && centreonRddProcess && expected)
            return true;
    
          if(expected == false && !centreonRddProcess && !centreonBrokerProcess )
            return false;

          await sleep(500)
        }
        
        return centreonBrokerProcess != undefined && centreonRddProcess != undefined;
    }



  

    static async getConfig(): Promise<JSON> {
      return JSON.parse((await fs.readFile('/etc/centreon-broker/central-broker.json')).toString());
    }

    static async writeConfig(config: JSON) {
        await fs.writeFile('/etc/centreon-broker/central-broker.json', JSON.stringify(config, null, '\t'))
    }

    static  resetConfig() {
      return shell.cp(path.join(__dirname, '../config/centreon-broker.json'), Broker.CENTRON_BROKER_CONFIG_PATH)
    }


    static async checkLogFileContanin(strings: Array<string>, seconds = 15): Promise<Boolean> {

      for (let i = 0; i < seconds * 10; ++i) {
        const logs = await Broker.getLogs()

        if (logs.includes(strings[0])) {
          return true;
        }

        await sleep(100)
      }

     throw Error(`log file ${Broker.CENTREON_BROKER_LOGS_PATH} do not contain expected strings ${strings.toString()}`)
    }

    static async getLogs() {
      return ( await fs.readFile(Broker.CENTREON_BROKER_LOGS_PATH)).toString()
    }

    static clearLogs() {
      shell.rm(Broker.CENTREON_BROKER_LOGS_PATH)
    }
}