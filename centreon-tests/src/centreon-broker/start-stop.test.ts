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

    const isStarted = await broker.start();
    expect(isStarted).toBeTruthy()

    const isStopped = await broker.stop()
    expect(isStopped).toBeTruthy();
    
    // expect(await Broker.getLogs()).not.toContain("error")
  }, 60000);

  it('start and stop many instances broker with .3sec interval', async () => {
    for(let i = 0; i < 10; ++i) {
      const broker = new Broker();

      const isStarted = await broker.start();
      expect(isStarted).toBeTruthy()

      const isStopped = await broker.stop()
      expect(isStopped).toBeTruthy();

      await sleep(300)

      const coreDumpResult = shell.exec('coredumpctl');
      expect(coreDumpResult.code).toBe(1);
      expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
      expect(0).toBe(0)

      // TODO: should not contain any error
      // expect(await Broker.getLogs()).not.toContain("error")
    }
  }, 240000)


  it('start and stop many instances broker with 1sec sleep interval', async () => {

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

      // TODO: should not contain any error
      // expect(await Broker.getLogs()).not.toContain("error")
    }
  }, 240000)
});
