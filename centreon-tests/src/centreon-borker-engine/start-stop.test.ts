import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';
import { isBorkerAndEngineConnected } from '../core/brokerEngine';
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
  

  it('start/stop centreon broker - broker first', async () => {

    const broker = new Broker();
    expect(await broker.start()).toBeTruthy()

    const engine = new Engine()
    expect(await engine.start()).toBeTruthy()

    
    expect(await isBorkerAndEngineConnected()).toBeTruthy()

    
    expect(await engine.stop()).toBeTruthy();
    expect(await engine.start()).toBeTruthy()


    expect(await isBorkerAndEngineConnected()).toBeTruthy()

   
    expect(await engine.stop()).toBeTruthy();
    expect(await broker.stop()).toBeTruthy();
  }, 60000);


  it('start/stop centreon broker - engine first', async () => {
    
    const engine = new Engine()
    expect(await engine.start()).toBeTruthy()

    const broker = new Broker();
    expect(await broker.start()).toBeTruthy()


    expect(await isBorkerAndEngineConnected()).toBeTruthy()

    expect(await broker.stop()).toBeTruthy();
    expect(await broker.start()).toBeTruthy()


    expect(await isBorkerAndEngineConnected()).toBeTruthy()

    expect(await broker.stop()).toBeTruthy();
    expect(await engine.stop()).toBeTruthy();
  }, 60000);

});
