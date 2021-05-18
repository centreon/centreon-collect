import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { broker } from '../shared';
import { getConfig, writeConfig } from '../config';
import fs from 'fs/promises'
shell.config.silent = true;


const repeat = async (times: number, interval: number, cond: () => boolean) => {

  let condResult;


  return false;
}

beforeEach(async () => {
  await broker.stop()
  shell.rm('/var/log/centreon-broker/central-broker-master.log')
}, 30000)


afterAll(async () => {
  await broker.stop()
  shell.rm('/var/log/centreon-broker/central-broker-master.log')
}, 30000)

describe('Bad credentials', () => {
  it('should deny access when database user password is wrong', async () => {


    const config = await getConfig()

    const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
    const centrealBorkerMasterperfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))

    centrealBorkerMasterSql['db_password'] = "centreon1"
    centrealBorkerMasterperfData['db_password'] = "centreon1"

    await writeConfig(config)


    await broker.start();

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

    // reset config to default for vm user
    centrealBorkerMasterSql['db_password'] = "centreon"
    centrealBorkerMasterperfData['db_password'] = "centreon"

    await writeConfig(config)

  }, 30000);

  it('should deny access when database user password is wrong for storage', async () => {

    const config = await getConfig()
    const centrealBorkerMasterperfData = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-perfdata'))
    centrealBorkerMasterperfData['db_password'] = "centreon1"
    await writeConfig(config)


    await broker.start();

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

    // reset config to default for vm user
    centrealBorkerMasterperfData['db_password'] = "centreon"
    await writeConfig(config)

  }, 30000);


  it('should deny access when database user password is wrong for sql', async () => {

    const config = await getConfig()
    const centrealBorkerMasterSql = config['centreonBroker']['output'].find((output => output.name === 'central-broker-master-sql'))
    centrealBorkerMasterSql['db_password'] = "centreon1"
    await writeConfig(config)


    await broker.start();

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

    // reset config to default for vm user
    centrealBorkerMasterSql['db_password'] = "centreon"
    await writeConfig(config)

  }, 30000);
});

