import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Engine } from '../core/engine';
import { Broker } from '../core/broker';

shell.config.silent = true;

describe("start and stop engine", () => {
    beforeEach(() => {
      /* closes cbd if running */
      if (Broker.isCbdAlreadyRunning()) {
        shell.exec('systemctl stop cbd')
      }

      /* close instance of centengine if running */
      if (Engine.isCentengineAlreadyRunning()) {
        shell.exec('systemctl stop centengine')
      }

      /* closes instances of cbd if running */
      if (Broker.isCbdInstancesRunning()) {
        Broker.closeCbdInstances()
      }

      /* closes instances of centengine if running */
      if (Engine.isCentengineInstancesRunning()) {
          Engine.closeCentengineInstances()
      }
    })

    afterAll(() => {
        beforeEach(() => {
          /* closes cbd if running */
          if (Broker.isCbdAlreadyRunning()) {
            shell.exec('systemctl stop cbd')
          }

          /* close instance of centengine if running */
          if (Engine.isCentengineAlreadyRunning()) {
            shell.exec('systemctl stop centengine')
          }

          /* closes instances of cbd if running */
          if (Broker.isCbdInstancesRunning()) {
            Broker.closeCbdInstances()
          }

          /* closes instances of centengine if running */
          if (Engine.isCentengineInstancesRunning()) {
              Engine.closeCentengineInstances()
          }
        })
    })


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
