import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { broker } from '../shared';

shell.config.silent = true;

describe('broker testing', () => {

  beforeEach(async () => {
    await broker.stop()
    shell.rm('/var/log/centreon-broker/central-broker-master.log')
  })

  
  it('start/stop centreon broker', async () => {
    await broker.start();
    await broker.stop(); 
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
