
import shell from 'shelljs'
import psList from 'ps-list'
import sleep from 'await-sleep'
import  { once } from 'events'
import { ChildProcess } from 'child_process'

export class Engine {
    private process: ChildProcess
    private config: JSON;
    static CENTREON_ENGINE_UID = parseInt(shell.exec('id -u centreon-engine'))
    static CENTRON_ENGINE_CONFIG_PATH = `/etc/centreon-engine/centengine.cfg`

    constructor() {

    }

    /**
     * this function will start a new centreon engine
     * upon completition
     * 
     * @returns Promise<Boolean> true if correctly started, else false
     */
    async start() {
        this.process = shell.exec(`/usr/sbin/centengine ${Engine.CENTRON_ENGINE_CONFIG_PATH}`, {async: true, uid: Engine.CENTREON_ENGINE_UID})  
    
        const isRunning = await this.isRunning(true, 20)
        return isRunning;
      }


    
    /**
     * will stop current engine instance if already running
     * 
     * @returns Promise<Boolean> true if correctly stoped, else false
     */
    async stop() {
      if(await this.isRunning(true, 5)) {
          this.process.kill()
          const isRunning = await this.isRunning(false)
          return !isRunning;
      }

      return true;
  }


   /**
     * this function will check the list of all process running in current os 
     * to check that the current instance of engine is correctly running or not
     * 
     * @param  {boolean=true} expected the expected value, true or false
     * @param  {number=15} seconds number of seconds to wait for process to show in processlist
     * @returns Promise<Boolean>
     */
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