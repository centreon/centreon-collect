import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
shell.config.silent = true;

export const broker = {
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
    expect(broker.isRunning()).toBeTruthy()

    return result;
  },
  stop: async () => {
    const result = shell.exec(`service cbd stop`);
    await sleep(5000);
    expect(broker.isRunning()).toBeTruthy()

    return result;
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
    await sleep(5000);

    expect(engine.isRunning()).toBeTruthy()
    return result;
  },
  stop: async() => {
    const result = shell.exec(`service centengine stop`);
    await sleep(5000);

    expect(engine.isRunning()).toBeFalsy()
    return result;
  },
};
