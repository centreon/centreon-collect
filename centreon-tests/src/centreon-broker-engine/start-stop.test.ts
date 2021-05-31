import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';
import { isBrokerAndEngineConnected } from '../core/brokerEngine';
import { broker } from 'shared';

shell.config.silent = true;

describe('engine and broker testing in same time', () => {

  beforeEach(() => {
    Broker.clearLogs()
    Broker.resetConfig()
  })

  afterAll(() => {
    beforeEach(() => {
      Broker.clearLogs()
      Broker.resetConfig()
    })
  })


  it('start/stop centreon broker/engine - broker first', async () => {

    const broker = new Broker(1);
    expect(await broker.start()).toBeTruthy()

    const engine = new Engine()
    expect(await engine.start()).toBeTruthy()

    expect(await isBrokerAndEngineConnected()).toBeTruthy()

    expect(await engine.stop()).toBeTruthy();
    expect(await engine.start()).toBeTruthy()

    expect(await isBrokerAndEngineConnected()).toBeTruthy()

    expect(await engine.stop()).toBeTruthy();
    expect(await broker.stop()).toBeTruthy();

    expect(await Broker.checkCoredump()).toBeFalsy()
    expect(await Engine.checkCoredump()).toBeFalsy()

  }, 60000);


  it('start/stop centreon broker/engine - engine first', async () => {
    const engine = new Engine()
    expect(await engine.start()).toBeTruthy()

    const broker = new Broker(1);
    expect(await broker.start()).toBeTruthy()

    expect(await isBrokerAndEngineConnected()).toBeTruthy()

    expect(await broker.stop()).toBeTruthy();
    expect(await broker.start()).toBeTruthy()

    expect(await isBrokerAndEngineConnected()).toBeTruthy()

    expect(await broker.stop()).toBeTruthy();
    expect(await engine.stop()).toBeTruthy();

    expect(await Broker.checkCoredump()).toBeFalsy()
    expect(await Engine.checkCoredump()).toBeFalsy()
  }, 60000);

});
