import sleep from "await-sleep";
import shell from "shelljs";
import { once } from "events";
import { Engine } from "../core/engine";
import { Broker } from "../core/broker";

shell.config.silent = true;

describe("start and stop engine", () => {
  beforeEach(() => {
    Broker.cleanAllInstances();
    Engine.cleanAllInstances();
  }, 30000);

  it("ESS1: start/stop centengine", async () => {
    const engine = new Engine();
    Engine.buildConfigs(50, 40);
    const started = await engine.start();
    const stopped = await engine.stop();

    Engine.cleanAllInstances();

    expect(started).toBeTruthy();
    expect(stopped).toBeTruthy();
    expect(await engine.checkCoredump()).toBeFalsy();
  }, 30000);

  it("ESS2: start and stop many instances engine", async () => {
    for (let i = 0; i < 5; ++i) {
      console.log(`Step ${i + 1}/10`);
      const engine = new Engine();
      const started = await engine.start();
      await sleep(300);
      const stopped = await engine.stop();

      await Engine.cleanAllInstances();

      expect(started).toBeTruthy();
      expect(stopped).toBeTruthy();
      expect(await engine.checkCoredump()).toBeFalsy();
    }
  }, 120000);
});
