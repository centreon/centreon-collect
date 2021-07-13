import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import fs from 'fs/promises'
import { readFile } from 'fs';
import { doesNotReject } from 'assert/strict';
shell.config.silent = true;

beforeEach(async () => {
    Broker.clearLogs();
}, 30000)

afterEach(async () => {
    Broker.resetConfig();
})

it('should deny access when database name exists but is not the good one for sql output', async () => {

    const config = await Broker.getConfig();
    const centralBrokerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
    centralBrokerMasterSql['db_name'] = "centreon"
    await Broker.writeConfig(config)

    const broker = new Broker();
    const isStarted = await broker.start();

    expect(isStarted).toBeTruthy()
    expect(await Broker.checkLogFileContains(["Table 'centreon.instances' doesn't exist"])).toBeTruthy()

    const isStopped = await broker.stop()
    console.log("isStopped = " + isStopped);
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
}, 120000);

it('should deny access when database name exists but is not the good one for storage output', async () => {

    const config = await Broker.getConfig()
    const centrealBrokerMasterPerfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
    centrealBrokerMasterPerfData['db_name'] = "centreon"
    await Broker.writeConfig(config)

    const broker = new Broker();
    expect(await broker.start()).toBeTruthy();

    expect(await Broker.checkLogFileContains(['[sql] [error] storage: rebuilder: Unable to connect to the database: storage: rebuilder: could not fetch index to rebuild'])).toBeTruthy()

    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
}, 30000);

it('should deny access when database name does not exists for sql output', async () => {

    const config = await Broker.getConfig();
    const centralBrokerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
    centralBrokerMasterSql['db_name'] = "centreon1"
    await Broker.writeConfig(config)

    const broker = new Broker();
    const isStarted = await broker.start();

    expect(isStarted).toBeTruthy()
    expect(await Broker.checkLogFileContains(['[core] [error] failover: global error: mysql_connection: error while starting connection'])).toBeTruthy()

    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()

}, 120000);

it('should deny access when database name does not exist for storage output', async () => {

    const config = await Broker.getConfig()
    const centrealBrokerMasterPerfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
    centrealBrokerMasterPerfData['db_name'] = "centreon1"
    await Broker.writeConfig(config)

    const broker = new Broker();
    expect(await broker.start()).toBeTruthy();

    expect(await Broker.checkLogFileContains(['[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection'])).toBeTruthy()

    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
}, 30000);

it('should deny access when database user password is wrong for sql', async () => {
    const config = await Broker.getConfig()
    const centralBrokerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
    centralBrokerMasterSql['db_password'] = "centreon1"
    await Broker.writeConfig(config)

    const broker = new Broker();
    const isStarted = await broker.start();

    expect(await broker.isRunning()).toBeTruthy()

    expect(await Broker.checkLogFileContains(['[core] [error] failover: global error: mysql_connection: error while starting connection'])).toBeTruthy()

    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()

}, 30000);

it('should log error when database name is not correct', async () => {

    const config = await Broker.getConfig()
    const centralBrokerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
    centralBrokerMasterSql['db_name'] = "centreon1"
    await Broker.writeConfig(config)
    //shell.config.silent = true;

    const broker = new Broker();
    const isStarted = await broker.start();

    expect(await broker.isRunning()).toBeTruthy()

    expect(await Broker.checkLogFileContains(['[core] [error] failover: global error: mysql_connection: error while starting connection'])).toBeTruthy()

    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
}, 60000)

it('multi connections step 1', async () => {
    const config = await Broker.getConfig()
    const centralBrokerMasterSql = config['centreonBroker']['output'].find((
        output => output.name === 'central-broker-master-sql'))
    centralBrokerMasterSql.connections_count = "4"

    const centralBrokerMasterPerfdata = config['centreonBroker']['output'].find((
        output => output.name === 'central-broker-master-perfdata'))
    centralBrokerMasterPerfdata.connections_count = "4"

    const loggers = config['centreonBroker']['log']['loggers']
    loggers['sql'] = "info"

    await Broker.writeConfig(config)

    const broker = new Broker();
    expect(await broker.start()).toBeTruthy()

    expect(await Broker.checkLogFileContains(['[sql] [info] mysql connector configured with 4 connection(s)'])).toBeTruthy()
    expect(await broker.stop()).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
}, 60000)

it('multi connections step 2', async () => {
    const config = await Broker.getConfig()
    const centralBrokerMasterSql = config['centreonBroker']['output'].find((
        output => output.name === 'central-broker-master-sql'))
    centralBrokerMasterSql.connections_count = "5"

    const centralBrokerMasterPerfdata = config['centreonBroker']['output'].find((
        output => output.name === 'central-broker-master-perfdata'))
    centralBrokerMasterPerfdata.connections_count = "5"

    const loggers = config['centreonBroker']['log']['loggers']
    loggers['sql'] = "info"

    await Broker.writeConfig(config)

    const broker = new Broker();
    expect(await broker.start()).toBeTruthy()

    expect(await Broker.checkLogFileContains(['[sql] [info] mysql connector configured with 5 connection(s)'])).toBeTruthy()
    expect(await broker.stop()).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
}, 60000)

it('mariadb server down', async () => {
    const broker = new Broker();
    expect(await broker.start()).toBeTruthy()

    for (let i = 0; i < 10; ++i) {
        shell.exec('service mysqld stop')
        await sleep(10000)
        shell.exec('service mysqld start')
        await sleep(10000)
    }

    expect(await broker.isRunning()).toBeTruthy()
    expect(await broker.stop()).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
}, 300000)

it('repeat 20 times start/stop cbd with a wrong configuration in perfdata', async () => {
    const config = await Broker.getConfig()
    const centralBrokerMasterPerfdata = config['centreonBroker']['output'].find((
        output => output.name === 'central-broker-master-perfdata'))
    centralBrokerMasterPerfdata['db_host'] = "1.2.3.4"
    await Broker.writeConfig(config)

    const broker = new Broker();
    for (let i = 0; i < 20; ++i) {
        const isStarted = await broker.start();
        expect(isStarted).toBeTruthy()
        expect(await broker.isRunning()).toBeTruthy()
        await sleep(2000);
        const isStopped = await broker.stop()
        expect(isStopped).toBeTruthy();
        expect(await broker.checkCoredump()).toBeFalsy()
    }
    let p = new Promise((resolve, reject) => {
        readFile(Broker.CENTREON_BROKER_LOGS_PATH, (err, data) => {
            if (err) {
                reject(err);
                return;
            }
            if (data) {
                if (data.toString().includes('[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection')) {
                    resolve(true);
                    return;
                }
            }
        });
    });
    expect(p.then((value) => { return value; }).catch(() => { return false; })).toBeTruthy();
}, 300000)

it('repeat 20 times start/stop cbd with a wrong configuration in sql', async () => {
    const config = await Broker.getConfig()
    const centralBrokerMasterSql = config['centreonBroker']['output'].find((
        output => output.name === 'central-broker-master-sql'))
    centralBrokerMasterSql['db_host'] = "1.2.3.4"
    await Broker.writeConfig(config)

    const broker = new Broker();
    for (let i = 0; i < 20; ++i) {
        const isStarted = await broker.start();
        expect(isStarted).toBeTruthy()
        expect(await broker.isRunning()).toBeTruthy()
        await sleep(2000)
        const isStopped = await broker.stop()
        expect(isStopped).toBeTruthy();
        expect(await broker.checkCoredump()).toBeFalsy()
    }
    let p = new Promise((resolve, reject) => {
        readFile(Broker.CENTREON_BROKER_LOGS_PATH, (err, data) => {
            if (err) {
                reject(err);
                return;
            }
            if (data) {
                if (data.toString().includes('[sql] [error] conflict_manager: not initialized after 10s. Probably an issue in the sql output configuration')) {
                    resolve(true);
                    return;
                }
            }
        });
    });
    expect(p.then((value) => { return value; }).catch(() => { return false; })).toBeTruthy();

}, 300000)