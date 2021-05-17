import sleep from 'await-sleep';
import shell from 'shelljs';

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
  }, 50000);


});
