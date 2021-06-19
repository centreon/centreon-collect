
import shell from 'shelljs'
import psList from 'ps-list'
import sleep from 'await-sleep'
import { once } from 'events'
import { ChildProcess } from 'child_process'
import { closeSync, existsSync, fstat, mkdir, mkdirSync, open, openSync, rmdir, rmdirSync, rmSync, write, writeSync } from 'fs'
import { rejects } from 'assert/strict'

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
        this.process = shell.exec(`/usr/sbin/centengine ${Engine.CENTRON_ENGINE_CONFIG_PATH}`, { async: true, uid: Engine.CENTREON_ENGINE_UID })

        const isRunning = await this.isRunning(true, 20)
        return isRunning;
    }


    /**
     * will stop current engine instance if already running
     *
     * @returns Promise<Boolean> true if correctly stoped, else false
     */
    async stop() {
        if (await this.isRunning(true, 5)) {
            this.process.kill()
            const isRunning = await this.isRunning(false)
            return !isRunning;
        }

        return true;
    }

    async checkCoredump(): Promise<boolean> {
        const cdList = await shell.exec('/usr/bin/coredumpctl').stdout.split('\n')
        let retval;
        retval = cdList.find(line => line.includes('cbd') &&
            line.includes(this.process.pid + ""));
        if (retval)
            return true;
        else
            return false;
    }

    /**
      * this function will check the list of all process running in current os
      * to check that the current instance of engine is correctly running or not
      *
      * @param  {boolean=true} expected the expected value, true or false
      * @param  {number=15} seconds number of seconds to wait for process to show in processlist
      * @returns Promise<Boolean>
      */
    async isRunning(expected: boolean = true, seconds: number = 15): Promise<boolean> {
        let centreonEngineProcess;

        for (let i = 0; i < seconds * 2; ++i) {

            const processList = await psList();
            centreonEngineProcess = processList.find((process) => process.pid == this.process.pid);

            if (centreonEngineProcess && expected)
                return true;

            else if (!centreonEngineProcess && !expect)
                return false;

            await sleep(500)
        }

        return !!centreonEngineProcess;
    }

    static createHost(id: number): string {
        let a = id % 255;
        let q = Math.floor(id / 255);
        let b = q % 255;
        q = Math.floor(q / 255);
        let c = q % 255;
        q = Math.floor(q / 255);
        let d = q % 255;

        let retval = `define host {                                                                   
    host_name                      host_${id}
    alias                          host_${id}
    address                        ${a}.${b}.${c}.${d}
    check_command                  check
    check_period                   24x7
    register                       1
    _KEY${id}                      VAL${id}
    _SNMPCOMMUNITY                 public                                       
    _SNMPVERSION                   2c                                           
    _HOST_ID                       ${id}
}
`;
        return retval;
    }

    static createCommand(commandId: number): string {
        if (commandId % 2 == 0) {
            let retval = `define command {
    command_name                    command_${commandId}
    command_line                    /var/lib/centreon-engine/check.pl ${commandId}
    connector                       Perl Connector
}
`;
            return retval
        }
        else {
            let retval = `define command {
    command_name                    command_${commandId}
    command_line                    /var/lib/centreon-engine/check.pl ${commandId}
}
`;
            return retval
        }
    }

    static createService(hostId: number, serviceId: number, nbCommands: number): string {
        let commandId = ((hostId + 1) * (serviceId + 1)) % nbCommands;
        let retval = `define service {
    host_name                       host_${hostId}
    service_description             service_${serviceId}
    _SERVICE_ID                     ${serviceId}
    check_command                   command_${commandId}
    max_check_attempts              3
    check_interval                  5
    retry_interval                  5
    register                        1
    active_checks_enabled           1
    passive_checks_enabled          1
}
`
        return retval;
    }

    static async buildConfig(hosts: number = 50, servicesByHost: number = 20): Promise<boolean> {
        let nbCommands = 50;
        let configDir = process.cwd() + '/src/config/centreon-engine';
        if (existsSync(configDir)) {
            rmSync(configDir, { recursive: true });
        }

        let p = new Promise((resolve, reject) => {
            let count = hosts + hosts * servicesByHost + nbCommands;
            mkdir(configDir, () => {
                open(configDir + '/hosts.cfg', 'w', (err, fd) => {
                    if (err) {
                        reject(err);
                        return;
                    }
                    for (let i = 1; i <= hosts; ++i) {
                        write(fd, Buffer.from(Engine.createHost(i)), (err) => {
                            --count;    // one host written
                            if (count <= 0) {
                                resolve(true);
                                return;
                            }

                            if (err) {
                                reject(err);
                                return;
                            }

                            open(configDir + '/services.cfg', 'a', (err, fd) => {
                                if (err) {
                                    reject(err);
                                    return;
                                }
                                for (let j = 1; j <= servicesByHost; ++j) {
                                    write(fd, Buffer.from(Engine.createService(i, j, nbCommands)), (err) => {
                                        if (err) {
                                            reject(err);
                                            return;
                                        }
                                        --count;    // One service written
                                        if (count <= 0) {
                                            resolve(true);
                                            return;
                                        }
                                    });
                                }
                            });
                        });
                    }
                    open(configDir + '/commands.cfg', 'w', (err, fd) => {
                        for (let i = 1; i <= nbCommands; ++i) {
                            write(fd, Buffer.from(Engine.createCommand(i)), (err) => {
                                if (err) {
                                    reject(err);
                                    return;
                                }
                                --count;    // One command written
                                if (count <= 0) {
                                    resolve(true);
                                    return;
                                }
                            });
                        }
                    });
                });
                if (count <= 0)
                    resolve(true);
            });
        });

        let retval = p.then(ok => { return true; })
            .catch(err => {
                console.log(err);
                return false
            });
        return retval;
    }
}
