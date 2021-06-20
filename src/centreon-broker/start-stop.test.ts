import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';

//shell.config.silent = true;

describe('broker testing', () => {

    beforeEach(() => {
        Broker.clearLogs()
    })

    it('start/stop centreon broker => no coredump', async () => {
        const broker = new Broker();

        const isStarted = await broker.start();
        expect(isStarted).toBeTruthy()

        const isStopped = await broker.stop()
        expect(isStopped).toBeTruthy();

        expect(await broker.checkCoredump()).toBeFalsy()
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
