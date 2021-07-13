import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Engine } from '../core/engine';

shell.config.silent = true;

describe("start and stop engine", () => {
    it('start/stop centengine', async () => {
        const engine = new Engine()
        const isStarted = await engine.start();
        expect(isStarted).toBeTruthy();

        const isStoped = await engine.stop();
        expect(isStoped).toBeTruthy();
        expect(await engine.checkCoredump()).toBeFalsy()
    }, 30000);

    it('start and stop many instances engine', async () => {

        for (let i = 0; i < 3; ++i) {
            const engine = new Engine()
            const isStarted = await engine.start();
            expect(isStarted).toBeTruthy();

            await sleep(300)
            const isStoped = await engine.stop();
            expect(isStoped).toBeTruthy();
            expect(await engine.checkCoredump()).toBeFalsy()
        }
    }, 120000);

    //it('Check custom config', async () => {
    //    expect(await Engine.buildConfig()).toBeTruthy();
    //}, 120000)
})
