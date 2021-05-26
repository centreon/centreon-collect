

import shell from 'shelljs'
import psList from 'ps-list'
import fs from 'fs/promises'
import { once } from 'events'
import { ChildProcess } from 'child_process'
import sleep from 'await-sleep';

export class Broker {
    private process: ChildProcess
    private rddProcess: ChildProcess
    private config: JSON;

    static CENTREON_BROKER_UID = parseInt(shell.exec('id -u centreon-broker'))
    static CENTREON_BROKER_LOGS_PATH = `/var/log/centreon-broker/central-broker-master.log`
    
    constructor() {
    }
    /**
     * this function will start a new centreon broker and rdd process
     * upon completition 
     */
    async start(): Promise<Boolean> {
        this.process = shell.exec(`/usr/sbin/cbd /etc/centreon-broker/central-broker.json`, {async: true, uid: Broker.CENTREON_BROKER_UID})
        this.rddProcess = shell.exec(`/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`, {async: true, uid: Broker.CENTREON_BROKER_UID})

        const isRunning = await this.isRunning()
        return isRunning;
    }

    async stop() {
        if(await this.isRunning(true, 5)) {
            this.process.kill()
            this.rddProcess.kill()

            await once(this.process, 'exit')
            await once(this.rddProcess, 'exit')

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

          
        }
        return !!centreonBrokerProcess;
    }


    static async getConfig(): Promise<JSON> {
      return JSON.parse((await fs.readFile('/etc/centreon-broker/central-broker.json')).toString());
    }
  

    static async checkLogFileContanin(strings: Array<string>, seconds = 15): Promise<Boolean> {

      for (let i = 0; i < seconds * 10; ++i) {
        const logs = await Broker.getLogs()


        console.log(logs);
        console.log("called")

        if (logs.includes(strings[0])) {
          return true;
        }

        await sleep(100)
      }

     throw Error(`log file ${Broker.CENTREON_BROKER_LOGS_PATH} do not contain expected strings ${strings.toString()}`)
    }

    static async writeConfig(config: JSON) {
        await fs.writeFile('/etc/centreon-broker/central-broker.json', JSON.stringify(config, null, '\t'))
    }

    static async getLogs() {
      return ( await fs.readFile(Broker.CENTREON_BROKER_LOGS_PATH)).toString()
    }


    static clearLogs() {
      shell.rm(Broker.CENTREON_BROKER_LOGS_PATH)
    }
}