import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';

shell.config.silent = true;

describe('broker testing', () => {

  beforeEach(() => {
    Broker.clearLogs()
  })

  it('start/stop centreon broker => no coredump', async () => {
    const broker = new Broker();

    const isStarted = await broker.start();
    expect(isStarted).toBeTruthy()

    console.log("started")
    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();

    expect(await Broker.checkCoredump()).toBeFalsy()
  }, 60000);

  it('repeat 10 times start/stop broker with .3sec interval => no coredump', async () => {
    for(let i = 0; i < 10; ++i) {
      const broker = new Broker();

      const isStarted = await broker.start();
      expect(isStarted).toBeTruthy()

      const isStopped = await broker.stop()
      expect(isStopped).toBeTruthy();

      await sleep(300)

      //const coreDumpResult = shell.exec('coredumpctl');
      //expect(coreDumpResult.code).toBe(1);
      //expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
      //expect(0).toBe(0)

      // TODO: should not contain any error
      // expect(await Broker.getLogs()).not.toContain("error")
    }
    expect(await Broker.checkCoredump()).toBeFalsy()
  }, 240000)


  it('repeat 10 times start/stop broker with 1sec interval => no coredump', async () => {

    for(let i = 0; i < 10; ++i) {
      const broker = new Broker();

      const isStarted = await broker.start();
      expect(isStarted).toBeTruthy()

      await sleep(1000)

      const isStopped = await broker.stop()
      expect(isStopped).toBeTruthy();

      const coreDumpResult = shell.exec('coredumpctl');
      expect(coreDumpResult.code).toBe(1);
      expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
      expect(0).toBe(0)
    }
    expect(await Broker.checkCoredump()).toBeFalsy()
  }, 240000)
});
