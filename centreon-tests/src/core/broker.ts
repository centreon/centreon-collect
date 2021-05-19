
import shell from 'shelljs'
import psList from 'ps-list'
import sleep from 'await-sleep'
import  { once } from 'process'


export class Broker {
    private process: any
    private config: JSON;

    constructor() {

    }

    async start() {
        this.process = shell.exec(`sh -c '/usr/sbin/cbd /etc/centreon-broker/central-broker.json'`, {async: true})
    }


    async stop() {
        
        if(await this.isRunning(true, 5)) {
            this.process.kill()
            await once('exit', this.process);
        }
    }


    async  isRunning(expected: boolean = true, seconds: number = 15) : Promise<boolean> {
        let centreonBrokerProcess;
        let centreonBrokerRddProcesss;
  
        for(let i = 0; i < seconds * 2; ++i) {
    
          const processList = await psList();
    
          centreonBrokerProcess = processList.find((process) => process.name == `cbd` 
            && process.ppid == this.process.pid 
            && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-broker.json`);

          centreonBrokerRddProcesss = processList.find((process) => process.name == `cbd` && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`);
    
          if((centreonBrokerProcess && centreonBrokerRddProcesss) && expected)
            return true;
    
          if(expected == false && !centreonBrokerProcess && !centreonBrokerRddProcesss)
            return false;

          
          await sleep(500)
        }
        return !!centreonBrokerProcess && !!centreonBrokerRddProcesss;
    }

    static clearLogs() {
      shell.rm('/var/log/centreon-broker/central-broker-master.log')
    }
}