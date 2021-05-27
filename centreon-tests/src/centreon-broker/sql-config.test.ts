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

it('should deny access when database user password is not corrrect', async () => {

  const config = await Broker.getConfig();
  const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
  const centrealBorkerMasterperfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
  centrealBorkerMasterSql['db_password'] = "centreon1"
  centrealBorkerMasterperfData['db_password'] = "centreon1"
  await Broker.writeConfig(config)

  const broker = new Broker();
  const isStarted = await broker.start();

  expect(isStarted).toBeTruthy()
  expect(await Broker.checkLogFileContanin(['[core] [error] failover: global error: mysql_connection: error while starting connection'])).toBeTruthy()
  
  const isStopped = await broker.stop()
  expect(isStopped).toBeTruthy();

}, 120000);

it('should deny access when database user password is wrong for storage', async () => {

  const config = await Broker.getConfig()
  const centrealBorkerMasterperfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
  centrealBorkerMasterperfData['db_password'] = "centreon1"
  await Broker.writeConfig(config)


  const broker = new Broker();
  const isStarted = await broker.start();
  expect(await broker.isRunning()).toBeTruthy()

  expect(await Broker.checkLogFileContanin(['[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection'])).toBeTruthy()
  
  const isStopped = await broker.stop()
  expect(isStopped).toBeTruthy();

}, 30000);

it('should deny access when database user password is wrong for sql', async () => {

  const config = await Broker.getConfig()
  const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
  centrealBorkerMasterSql['db_password'] = "centreon1"
  await Broker.writeConfig(config)


  const broker = new Broker();
  const isStarted = await broker.start();

  expect(await broker.isRunning()).toBeTruthy()

  expect(await Broker.checkLogFileContanin(['[core] [error] failover: global error: mysql_connection: error while starting connection'])).toBeTruthy()
  
  const isStopped = await broker.stop()
  expect(isStopped).toBeTruthy();
  
}, 30000);


it('should log error when databse name is not correct', async () => {

  const config = await Broker.getConfig()
  const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
  centrealBorkerMasterSql['db_name'] = "centreon1"
  await Broker.writeConfig(config)

  const broker = new Broker();
  const isStarted = await broker.start();

  expect(await broker.isRunning()).toBeTruthy()

  expect(await Broker.checkLogFileContanin(['[core] [error] failover: global error: mysql_connection: error while starting connection'])).toBeTruthy()
  
  const isStopped = await broker.stop()
  expect(isStopped).toBeTruthy();
}, 60000)


it("should handle database service stop and start", () => {
  shell.exec('service mysql stop')
})

