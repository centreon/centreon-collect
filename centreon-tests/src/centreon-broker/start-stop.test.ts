import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
shell.config.silent = true;

const broker = {
  isRunning: async () => {
    const result = shell.exec(`ps -ax | grep cbd`);

    const resultString = result.toString();

    // return code is different than 0 when already stop
    // error kill in script killing cbd
    return (
      resultString.toLocaleLowerCase().includes('central-broker.json') &&
      resultString.toLocaleLowerCase().includes('central-rrd.json')
    );
  },
  start: async () => {
    const result = shell.exec(`service cbd start`);
    await sleep(5000);

    return result;
  },
  stop: async () => {
    const result = shell.exec(`service cbd stop`);
    await sleep(5000);

    return result;
  },
};

const engine = {
  isRunning: () => {
    const result = shell.exec(`sh -c 'ps -ax | grep centengine'`);

    const resultString = result.toString();

    // return code is different than 0 when already stop
    // error kill in script killing centegine
    return resultString.toLocaleLowerCase().includes('centengine.cfg');
  },
  start: () => {
    const result = shell.exec(`service centengine start`);
    return result;
  },
  stop: () => {
    const result = shell.exec(`service centengine stop`);
    return result;
  },
};

describe('broker testing', () => {
  beforeEach(async () => {
    if (await broker.isRunning()) {
      const result = await broker.stop();
      expect(result.code).toBe(0);
    }


    expect(await broker.isRunning()).toBeFalsy();

  });

  it('start/stop centreon broker', async () => {

    const result = await broker.start();
    expect(result.code).toBe(0);

    expect(await broker.isRunning()).toBeTruthy();

    const startTime = process.hrtime();

    const brokerTimedStopResult = await broker.stop();
    expect(brokerTimedStopResult.code).toBe(0);

    const endTime = process.hrtime();

    const isBrokerRunning = await broker.isRunning();
    expect(isBrokerRunning).toBeFalsy();
  }, 30000);

  it('start and stop many instances broker with .3 seconds interval', async () => {

    for(let i = 0; i < 10; ++i) {
      const process = shell.exec(`su centreon-broker -c '/usr/sbin/cbd /etc/centreon-broker/central-broker.json'`, {async: true})
      await sleep(300)
      process.kill('SIGINT')
      await once(process, 'exit');  
    }

    const coreDumpResult = shell.exec('coredumpctl');
    expect(coreDumpResult.code).toBe(1);
    expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
    expect(0).toBe(0)
  }, 60000)

  it('start and stop many instances broker with 1 second interval', async () => {

    for(let i = 0; i < 10; ++i) {
      const process = shell.exec(`su centreon-broker -c '/usr/sbin/cbd /etc/centreon-broker/central-broker.json'`, {async: true})
      await sleep(1000)
      process.kill('SIGINT')
      await once(process, 'exit');  
    }

    const coreDumpResult = shell.exec('coredumpctl');
    expect(coreDumpResult.code).toBe(1);
    expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
    expect(0).toBe(0)
  }, 60000)
});
