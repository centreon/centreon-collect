import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { getConfig, writeConfig } from '../core/config';
import fs from 'fs/promises'
shell.config.silent = true;

beforeEach(async () => {
  Broker.clearLogs()
}, 30000)

it('should deny access when database user password is wrong', async () => {

  const config = await getConfig()
  const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
  const centrealBorkerMasterperfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
  centrealBorkerMasterSql['db_password'] = "centreon1"
  centrealBorkerMasterperfData['db_password'] = "centreon1"
  await writeConfig(config)

  const broker = new Broker();
  await broker.start();

  expect(await broker.isRunning()).toBeTruthy()

  let logFile;
  let logFileString;

  for (let i = 0; i < 10; ++i) {
    logFile = await fs.readFile('/var/log/centreon-broker/central-broker-master.log')
    logFileString = logFile.toString()

    if (logFileString.includes("[core] [error] failover: global error: mysql_connection: error while starting connection") && 
    logFileString.includes("[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection") )
      break;

    await sleep(1000)
  }

  expect(logFileString).toContain(`[core] [error] failover: global error: mysql_connection: error while starting connection`);
  expect(logFileString).toContain(`[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection`);

  await broker.stop()
  expect(await broker.isRunning(false)).toBeFalsy();

  // reset config to default for vm user
  centrealBorkerMasterSql['db_password'] = "centreon"
  centrealBorkerMasterperfData['db_password'] = "centreon"
  
  await writeConfig(config)

}, 60000);

it('should deny access when database user password is wrong for storage', async () => {

  const config = await getConfig()
  const centrealBorkerMasterperfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
  centrealBorkerMasterperfData['db_password'] = "centreon1"
  await writeConfig(config)


  const broker = new Broker();
  await broker.start();

  expect(await broker.isRunning()).toBeTruthy()

  let logFile;
  let logFileString;

  for (let i = 0; i < 10; ++i) {
    logFile = await fs.readFile('/var/log/centreon-broker/central-broker-master.log')
    logFileString = logFile.toString()

    if (logFileString.includes("[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection"))
      break;

    await sleep(1000)
  }

  expect(logFileString).toContain(`[sql] [error] storage: rebuilder: Unable to connect to the database: mysql_connection: error while starting connection`);


  await broker.stop()
  expect(await broker.isRunning(false)).toBeFalsy();


  // reset config to default for vm user
  centrealBorkerMasterperfData['db_password'] = "centreon"
  await writeConfig(config)

}, 30000);

it('should deny access when database user password is wrong for sql', async () => {

  const config = await getConfig()
  const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
  centrealBorkerMasterSql['db_password'] = "centreon1"
  await writeConfig(config)


  const broker = new Broker();
  await broker.start();

  expect(await broker.isRunning()).toBeTruthy()

  let logFile;
  let logFileString;

  for (let i = 0; i < 10; ++i) {
    logFile = await fs.readFile('/var/log/centreon-broker/central-broker-master.log')
    logFileString = logFile.toString()

    if (logFileString.includes("[core] [error] failover: global error: mysql_connection: error while starting connection"))
      break;

    await sleep(1000)
  }

  expect(logFileString).toContain(`[core] [error] failover: global error: mysql_connection: error while starting connection`);

  await broker.stop()
  expect(await broker.isRunning(false)).toBeFalsy();

  // reset config to default for vm user
  centrealBorkerMasterSql['db_password'] = "centreon"
  await writeConfig(config)

}, 30000);


it('should log error when databse name is not correct', async () => {
  const config = await getConfig()
  const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
  centrealBorkerMasterSql['db_name'] = "centreon1"
  await writeConfig(config)

  const broker = new Broker();
  await broker.start();

  expect(await broker.isRunning()).toBeTruthy()

  let logFile;
  let logFileString;

  for (let i = 0; i < 10; ++i) {
    logFile = await fs.readFile('/var/log/centreon-broker/central-broker-master.log')
    logFileString = logFile.toString()

    if (logFileString.includes("[core] [error] failover: global error: mysql_connection: error while starting connection"))
      break;

    await sleep(1000)
  }

  expect(logFileString).toContain(`[core] [error] failover: global error: mysql_connection: error while starting connection`);

  await broker.stop()
  expect(await broker.isRunning(false)).toBeFalsy();

  centrealBorkerMasterSql['db_name'] = "centreon"
  await writeConfig(config)
})


it("should handle database service stop and start", () => {
  shell.exec('service mysql stop')
})

