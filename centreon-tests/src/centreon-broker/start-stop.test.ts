import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';

shell.config.silent = true;

describe('broker testing', () => {
    beforeEach(() => {
        Broker.cleanAllInstances();
        Engine.cleanAllInstances();

        Broker.clearLogs()
    })

    afterAll(() => {
        beforeEach(() => {
          Broker.cleanAllInstances();
          Engine.cleanAllInstances();
        })
    })


    it('start/stop centreon broker => no coredump', async () => {
        const broker = new Broker();

        const isStarted = await broker.start();
        expect(isStarted).toBeTruthy()

        const isStopped = await broker.stop()
        expect(isStopped).toBeTruthy();

        expect(await broker.checkCoredump()).toBeFalsy()
    }, 60000);


    it('start/stop centreon broker with reversed connection on TCP acceptor but only this instance => no deadlock', async () => {
        
        /* Let's get the configuration, we remove the host to connect since we wan't the other peer
         * to establish the connection. We also set the one peer retention mode (just for the configuration
         * to be correct, not needed for the test). */
        const config = await Broker.getConfig();
        const centralBrokerMasterRRD = config['centreonBroker']['output'].find((output => output.name === 'centreon-broker-master-rrd'));
        delete centralBrokerMasterRRD.host;
        centralBrokerMasterRRD["one_peer_retention_mode"] = "yes";
        await Broker.writeConfig(config)

        const broker = new Broker(1);

        const isStarted = await broker.start();
        expect(isStarted).toBeTruthy();

        const isStopped = await broker.stop();
        expect(isStopped).toBeTruthy();
        expect(await broker.checkCoredump()).toBeFalsy();
    }, 60000);

    it('repeat 10 times start/stop broker with .3sec interval => no coredump', async () => {
        const broker = new Broker();
        for (let i = 0; i < 10; ++i) {
            const isStarted = await broker.start();
            expect(isStarted).toBeTruthy()

            await sleep(300)

            const isStopped = await broker.stop()
            expect(isStopped).toBeTruthy();

            expect(await broker.checkCoredump()).toBeFalsy()
        }
    }, 240000)


    it('repeat 10 times start/stop broker with 1sec interval => no coredump', async () => {

        const broker = new Broker();
        for (let i = 0; i < 10; ++i) {

            const isStarted = await broker.start();
            expect(isStarted).toBeTruthy()

            await sleep(1000)

            const isStopped = await broker.stop()
            expect(isStopped).toBeTruthy();

            expect(await broker.checkCoredump()).toBeFalsy()
        }
    }, 240000)
});
