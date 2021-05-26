
import shell from 'shelljs'
import psList from 'ps-list'
import sleep from 'await-sleep'
import  { once } from 'events'
import { ChildProcess } from 'child_process'

export class Engine {
    private process: ChildProcess
    private config: JSON;
    static CENTREON_ENGINE_UID = parseInt(shell.exec('id -u centreon-engine'))

    constructor() {

    }

    async start() {
        this.process = shell.exec(`/usr/sbin/centengine /etc/centreon-engine/centengine.cfg`, {async: true, uid: Engine.CENTREON_ENGINE_UID})  
    }


    async stop() {
        if(await this.isRunning()) {
            this.process.kill()
            await once(this.process, 'exit')
        }
    }


    async  isRunning(expected: boolean = true, seconds: number = 15) : Promise<boolean> {
      let centreonEngineProcess;   

      for(let i = 0; i < seconds * 2; ++i) {
  

        const processList = await psList();
          centreonEngineProcess = processList.find((process) => process.pid == this.process.pid);
    
          if(centreonEngineProcess && expected)
            return true;
    
          else if(!centreonEngineProcess && !expect)
            return false;

          await sleep(500)
      }

      return !!centreonEngineProcess;
  }
}