import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import {  Engine } from '../core/engine';

shell.config.silent = true;


describe("start and stop engine", () => {
  it('start/stop centengine', async () => {
    const engine = new Engine()
    await engine.start();

    expect(await engine.isRunning()).toBeTruthy();

    await engine.stop();
    
    expect(await engine.isRunning(false)).toBeFalsy()
  }, 30000);

  it('start and stop many instances engine', async () => {

    for(let i = 0; i < 5; ++i) {
      const engine = new Engine();
      await engine.start();
      expect(await engine.isRunning()).toBeTruthy()
      await engine.stop()
      expect(await engine.isRunning(false, 15)).toBeFalsy();
    }

    const coreDumpResult = shell.exec('coredumpctl');
    expect(coreDumpResult.code).toBe(1);
    expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
    expect(0).toBe(0)
  }, 120000)
})
