import { $, sleep } from 'zx';

const broker = {
  isRunning: async () => {
    const result = await $( `sh -c 'ps -ax | grep cbd'`);

    const resultString = result.toString();

    // return code is different than 0 when already stop
    // error kill in script killing cbd
    return (
      resultString.toLocaleLowerCase().includes('central-broker.json') &&
      resultString.toLocaleLowerCase().includes('central-rrd.json')
    );
  },
  start: async () => {
    const result = await $`service cbd start`;
    await sleep(1000);

    return result;
  },
  stop: async () => {
    const result = await $(`service cbd stop`);
    await sleep(1000);

    return result;
  },
};

const engine = {
  isRunning: async () => {
    const result = await $(`sh -c 'ps -ax | grep centengine'`);

    const resultString = result.toString();

    // return code is different than 0 when already stop
    // error kill in script killing centegine
    return resultString.toLocaleLowerCase().includes('centengine.cfg');
  },
  start: async () => {
    const result = await $(`service centengine start`);
    return result;
  },
  stop: async () => {
    const result = await $(`service centengine stop`);
    return result;
  },
};

describe('start and stop centreon broker', () => {
  beforeEach(async () => {
    if (await broker.isRunning()) {
      const result = await broker.stop();
      expect(result.code).toBe(0);
    }

    if (await engine.isRunning()) {
      const result = await broker.stop();
      expect(result.code).toBe(0);
    }
  });

  it('start centreon broker', async () => {
    const result = await broker.start();
    expect(result.code).toBe(0);

    expect(await broker.isRunning()).toBeTruthy();
  }, 10000);

  it('stop centreon broker', async () => {
    expect(await broker.isRunning()).toBeFalsy();

    const brokerStartResult = await broker.start();
    expect(brokerStartResult.code).toBe(0);

    const isBrokerStarted = await broker.isRunning();
    expect(isBrokerStarted).toBeTruthy();

    const startTime = process.hrtime();

    const brokerTimedStopResult = await broker.stop();
    expect(brokerTimedStopResult.code).toBe(0);

    const endTime = process.hrtime();

    const isBrokerRunning = await broker.isRunning();
    expect(isBrokerRunning).toBeFalsy();
  }, 30000);
});
