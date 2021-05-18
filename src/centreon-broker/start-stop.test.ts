import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { broker } from '../utils';
shell.config.silent = true;

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
  }, 30000);

  it('start and stop many instances broker with .3 seconds interval', async () => {

    for(let i = 0; i < 5; ++i) {
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

    for(let i = 0; i < 5; ++i) {
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
