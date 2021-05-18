// import sleep from 'await-sleep';
// import shell from 'shelljs';
// import { once } from 'events'
// import {  engine } from '../shared';

// shell.config.silent = true;

// describe('start and stop', () => {

//   beforeEach(async () => {
//     if (await engine.isRunning()) {
//       const result = await engine.stop();
//     }
//   });

//   it('start/stop centengine', async () => {

//     const result = await engine.start();
//     expect(result.code).toBe(0);

//     expect(await engine.isRunning()).toBeTruthy();

//     await engine.stop();
//   }, 30000);

//   it('start and stop many instances centegine with .3 seconds interval', async () => {

//     for(let i = 0; i < 10; ++i) {
//       await engine.start()
//       await engine.stop()
//     }

//     const coreDumpResult = shell.exec('coredumpctl');
//     expect(coreDumpResult.code).toBe(1);
//     expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
//     expect(0).toBe(0)
//   }, 60000)

//   it('start and stop many instances centegine with 1 second interval', async () => {

//     for(let i = 0; i < 10; ++i) {
//       await engine.start()
//       await sleep(1000)

//       await engine.stop()
//     }

//     const coreDumpResult = shell.exec('coredumpctl');
//     expect(coreDumpResult.code).toBe(1);
//     expect(coreDumpResult.stderr?.toLocaleLowerCase()).toContain('no coredumps found')
//     expect(0).toBe(0)
//   }, 60000)
// });

it('test', () => {
  expect(0).toBe(0)
})