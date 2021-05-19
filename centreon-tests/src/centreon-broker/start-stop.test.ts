import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';

shell.config.silent = true;

describe('broker testing', () => {


  beforeEach(async () => {
  
  }, 60000)

  
  it('start/stop centreon broker', async () => {
    Broker.clearLogs()
    const broker = new Broker();

    await broker.start();

    expect(await broker.isRunning()).toBeTruthy()

    await broker.stop()

    expect(await broker.isRunning()).toBeTruthy();
  }, 60000);

  it('start and stop many instances broker with .3 seconds interval', async () => {

    for(let i = 0; i < 5; ++i) {
      const process = shell.exec(`su centreon-broker -c '/usr/sbin/cbd /etc/centreon-broker/central-broker.json'`, {async: true})
      await sleep(300)
      process.kill()
      await once(process, 'exit');  
    }

    const coreDumpResult = shell.exec('coredumpctl');
    expect(coreDumpResult.code).toBe(1);
    expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
    expect(0).toBe(0)
  }, 60000)

  it('start and stop many instances broker with 1 second interval', async () => {

    for(let i = 0; i < 5; ++i) {
      const process = shell.exec(`su  centreon-broker -c '/usr/sbin/cbd /etc/centreon-broker/central-broker.json'`, {async: true})
      await sleep(1000)
      process.kill()
      await once(process, 'exit');  
    }

    const coreDumpResult = shell.exec('coredumpctl');
    expect(coreDumpResult.code).toBe(1);
    expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
    expect(0).toBe(0)
  }, 60000)
});
