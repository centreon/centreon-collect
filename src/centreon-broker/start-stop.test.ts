import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';

shell.config.silent = true;

describe('broker testing', () => {

  beforeEach(() => {
    Broker.clearLogs()
  })

  it('start/stop centreon broker', async () => {
    const broker = new Broker();

    await broker.start();

    expect(await broker.isRunning()).toBeTruthy()

    await broker.stop()

    expect(await broker.isRunning(false)).toBeFalsy();
  }, 60000);

  it('start and stop many instances broker', async () => {

    for(let i = 0; i < 5; ++i) {
      const broker = new Broker();
      await broker.start();
      expect(await broker.isRunning()).toBeTruthy()
      await broker.stop()
      expect(await broker.isRunning(false)).toBeFalsy();
    }

    const coreDumpResult = shell.exec('coredumpctl');
    expect(coreDumpResult.code).toBe(1);
    expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
    expect(0).toBe(0)
  }, 120000)
});
