
import shell from 'shelljs'
import psList from 'ps-list'
import sleep from 'await-sleep'
import  { once } from 'process'


export class Broker {
    private process: any
    private rddProcess: any
    private config: JSON;

    static CENTREON_BROKER_UID = parseInt(shell.exec('id -u centreon-broker'))
    constructor() {

    }

    async start() {
        this.process = shell.exec(`/usr/sbin/cbd /etc/centreon-broker/central-broker.json`, {async: true, uid: Broker.CENTREON_BROKER_UID})
        this.rddProcess = shell.exec(`/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`, {async: true, uid: Broker.CENTREON_BROKER_UID})
    }


    async stop() {
        if(await this.isRunning(true, 5)) {
            this.process.kill()
            this.rddProcess.kill()
        }
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
        return !!centreonBrokerProcess;
    }

    static clearLogs() {
      shell.rm('/var/log/centreon-broker/central-broker-master.log')
    }
}