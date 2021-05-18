import sleep from 'await-sleep';
import shell, { cat } from 'shelljs';
import { once } from 'events'
import psList from 'ps-list';

shell.config.silent = true;



export const broker = {
  isRunning: async (repeat = 15, expected = true): Promise<Boolean> => {
    let centreonBrokerProcess;
    let centreonBrokerRddProcesss;

    for(let i = 0; i < repeat; ++i) {
      await sleep(1000)

      const processList = await psList();

      centreonBrokerProcess = processList.find((process) => process.name == `cbd` && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-broker.json`);
      centreonBrokerRddProcesss = processList.find((process) => process.name == `cbd` && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`);

      if((centreonBrokerProcess && centreonBrokerRddProcesss) && expected)
        return true;

      if(expected == false && !centreonBrokerProcess && !centreonBrokerRddProcesss)
        return false;
    }
    return !!centreonBrokerProcess && !!centreonBrokerRddProcesss;
  },

  start: async () => {
    const result = shell.exec(`service cbd start`);

    expect(result.code).toBe(0)
    expect(broker.isRunning()).toBeTruthy()
    return result;
  },
  stop: async () => {

    const result = shell.exec(`service cbd stop`)
    
      // const processList = await psList();

      // const centreonBrokerProcess = processList.find((process) => process.name == `cbd` && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-broker.json`);
      // const centreonBrokerRddProcesss = processList.find((process) => process.name == `cbd` && process.cmd === `/usr/sbin/cbd /etc/centreon-broker/central-rrd.json`);
  
      // if(centreonBrokerProcess) {
      //   process.kill(centreonBrokerProcess?.pid)
      // }
  
      // if(centreonBrokerRddProcesss) {
      //   process.kill(centreonBrokerRddProcesss?.pid)
      // }
  
      expect(result.code).toBe(0)
      expect(await broker.isRunning(15, false)).toBeFalsy()
  },
};

export const engine = {
  isRunning: () => {
    const result = shell.exec(`sh -c 'ps -ax | grep centengine'`);

    const resultString = result.toString();

    // return code is different than 0 when already stop
    // error kill in script killing centegine
    return resultString.toLocaleLowerCase().includes('centengine.cfg');
  },
  start: async () => {
    const result = shell.exec(`service centengine start`);
    expect(result.code).toBe(0);

    await sleep(5000);

    expect(engine.isRunning()).toBeTruthy()
    return result;
  },
  stop: async() => {
    const result = shell.exec(`service centengine stop`);
    expect(result.code).toBe(0);

    await sleep(5000);

    expect(engine.isRunning()).toBeFalsy()
    return result;
  },
};
