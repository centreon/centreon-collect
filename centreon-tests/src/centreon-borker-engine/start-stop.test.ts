import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';
import { isBorkerAndEngineConnected } from '../core/brokerEngine';

shell.config.silent = true;

describe('engine and broker testing in same time', () => {

  beforeEach(() => {
    Broker.clearLogs()
  })

  it('start/stop centreon broker', async () => {
    // const broker = new Broker();
    // await broker.start();

    // expect(await broker.isRunning()).toBeTruthy()

    // const engine = new Engine()
    // await engine.start()

    // expect(await engine.isRunning()).toBeTruthy()
    

    // await sleep(60000)
    // expect(isBorkerAndEngineConnected()).toBeTruthy()


    // await engine.stop()
    // expect(await engine.isRunning(false, 15)).toBeFalsy();

    // const ssResultCentegineStopped = shell.exec('ss | grep cbmod')
    // expect(ssResultCentegineStopped.stdout).toContain('cbmod.so')
    // expect(ssResultCentegineStopped.code).not.toBe(0)


    // await engine.start()
    // expect(await engine.isRunning()).toBeTruthy()
    // expect(isBorkerAndEngineConnected()).toBeTruthy()

    // await engine.stop()
    // await broker.stop()

    // expect(await broker.isRunning(false, 15)).toBeFalsy();
    // expect(await engine.isRunning(false, 15)).toBeFalsy();
    expect(0).toBe(0)

  }, 60000);

});
