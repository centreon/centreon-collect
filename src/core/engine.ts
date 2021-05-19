
import shell from 'shelljs'
import psList from 'ps-list'
import sleep from 'await-sleep'
import  { once } from 'process'

export class Engine {
    private process: any
    private config: JSON;

    constructor() {

    }

    async start() {
        this.process = shell.exec(`su centreon-broker -c '/usr/sbin/centengine /etc/centreon-engine/centegine.cfg'`, {async: true})
    }


    async stop() {
        if(await this.isRunning(5, true)) {
            this.process.kill()
            await once('exit', this.process);
        }
    }


    async isRunning(repeat: number = 15, expected: boolean = true) : Promise<boolean> {
        let centreonBrokerProcess;
        let centreonBrokerRddProcesss;
    
        for(let i = 0; i < repeat; ++i) {
    
          const processList = await psList();
    
          centreonBrokerProcess = processList.find((process) => process.name == `cbd` && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-broker.json`);
          centreonBrokerRddProcesss = processList.find((process) => process.name == `cbd` && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`);
    
          if((centreonBrokerProcess && centreonBrokerRddProcesss) && expected)
            return true;
    
          if(expected == false && !centreonBrokerProcess && !centreonBrokerRddProcesss)
            return false;

          await sleep(1000)
        }
        return !!centreonBrokerProcess && !!centreonBrokerRddProcesss;
    }
}