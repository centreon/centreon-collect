import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import fs from 'fs/promises'
shell.config.silent = true;

beforeEach(async () => {
  Broker.clearLogs()
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
  expect(await Broker.checkLogFileContains(['[core] [error] failover: global error: conflict_manager: events loop interrupted'])).toBeTruthy()

  const isStopped = await broker.stop()
  expect(isStopped).toBeTruthy();
  expect(await broker.checkCoredump()).toBeFalsy()

}, 120000);

it('should deny access when database name exists but is not the good one for storage output', async () => {

  const config = await Broker.getConfig()
  const centrealBrokerMasterPerfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
  centrealBrokerMasterPerfData['db_name'] = "centreon"
  await Broker.writeConfig(config)

  const broker = new Broker();
  const isStarted = await broker.start();
  expect(await broker.isRunning()).toBeTruthy()

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
  const isStarted = await broker.start();
  expect(await broker.isRunning()).toBeTruthy()

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

  const broker = new Broker();
  const isStarted = await broker.start();

  expect(await broker.isRunning()).toBeTruthy()

  expect(await Broker.checkLogFileContains(['[core] [error] failover: global error: mysql_connection: error while starting connection'])).toBeTruthy()

  const isStopped = await broker.stop()
  expect(isStopped).toBeTruthy();
  expect(await broker.checkCoredump()).toBeFalsy()
}, 60000)

it('repeat 20 times start/stop cbd with a wrong configuration in perfdata', async () => {
  const config = await Broker.getConfig()
  const centralBrokerMasterPerfdata = config['centreonBroker']['output'].find((
      output => output.name === 'central-broker-master-perfdata'))
  centralBrokerMasterPerfdata['db_host'] = "1.2.3.4"
  await Broker.writeConfig(config)

  const broker = new Broker();
  for(let i = 0; i < 20; ++i) {
    console.log(i)
    const isStarted = await broker.start();
    expect(isStarted).toBeTruthy()
    expect(await broker.isRunning()).toBeTruthy()
    await sleep(2000)
    expect(await Broker.checkLogFileContains(
      ['[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection'])).toBeTruthy()
    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
    expect(await broker.checkCoredump()).toBeFalsy()
  }
}, 220000)

it('repeat 20 times start/stop cbd with a wrong configuration in sql', async () => {
  const config = await Broker.getConfig()
  const centralBrokerMasterSql = config['centreonBroker']['output'].find((
      output => output.name === 'central-broker-master-sql'))
  centralBrokerMasterSql['db_host'] = "1.2.3.4"
  await Broker.writeConfig(config)

  const broker = new Broker();
  for(let i = 0; i < 20; ++i) {
    console.log(i)
    const isStarted = await broker.start();
    expect(isStarted).toBeTruthy()
    expect(await broker.isRunning()).toBeTruthy()
    await sleep(2000)
    expect(await Broker.checkLogFileContains(
      ['[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection'])).toBeTruthy()
    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
  }
  expect(await broker.checkCoredump()).toBeFalsy()
}, 220000)


//it("should handle database service stop and start", () => {
//   shell.exec('service mysql stop')
// })
