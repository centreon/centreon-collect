import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';
import { isBrokerAndEngineConnected } from '../core/brokerEngine';
import { broker } from 'shared';


shell.config.silent = true;

describe('engine and broker testing in same time for compression', () => {

    beforeEach(() => {
        Broker.clearLogs()
        Broker.clearLogsCentralModule()
        Broker.resetConfig()
        Broker.resetConfigCentralModule()
    })

    afterAll(() => {
        beforeEach(() => {
            Broker.clearLogs()
            Broker.resetConfig()
            Broker.resetConfigCentralModule()
        })
    })

    it('compression checks between broker - engine', async () => {
        const broker = new Broker()
        const engine = new Engine()

	const compression = ["yes", "no", "auto"]
        const errors = [
          // yes, yes
	  [
           "[bbdo] [info] BBDO: we have extensions \'TLS compression\' and peer has \'compression\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'compression\' and peer has \'TLS compression\'" // central side
          ], 
          // yes, no 
          [
           "[bbdo] [info] BBDO: we have extensions \'TLS compression\' and peer has \'\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'\' and peer has \'TLS compression\'" // central side
          ], 
          // yes, auto 
	  [
           "[bbdo] [info] BBDO: we have extensions \'TLS compression\' and peer has \'compression\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'compression\' and peer has \'TLS compression\'" // central side
          ], 
          // no, yes
          [
           "[bbdo] [info] BBDO: we have extensions \'TLS\' and peer has \'compression\'", 
           "[bbdo] [info] BBDO: we have extensions \'compression\' and peer has \'TLS\'"
          ],
          // no, no
          [
           "[bbdo] [info] BBDO: we have extensions \'TLS\' and peer has \'\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'\' and peer has \'TLS\'" // central side
          ], 
          // no, auto
	  [
           "[bbdo] [info] BBDO: we have extensions \'TLS\' and peer has \'compression\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'compression\' and peer has \'TLS\'" // central side
          ],
          // auto, yes
          [
           "[bbdo] [info] BBDO: we have extensions \'TLS compression\' and peer has \'compression\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'compression\' and peer has \'TLS compression\'" // central side
          ],
          // auto, no
          [
           "[bbdo] [info] BBDO: we have extensions \'TLS compression\' and peer has \'\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'\' and peer has \'TLS compression\'" // central side
          ], 
          // auto, auto
          [
           "[bbdo] [info] BBDO: we have extensions \'TLS compression\' and peer has \'compression\'", // broker side
           "[bbdo] [info] BBDO: we have extensions \'compression\' and peer has \'TLS compression\'" // central side
          ],
        ]

        const config_broker = await Broker.getConfig()
        const config_module = await Broker.getConfigCentralModule()

        const centralModuleLoggers = config_module['centreonBroker']['log']['loggers']
        const centralBrokerLoggers = config_broker['centreonBroker']['log']['loggers']

        centralModuleLoggers['bbdo'] = "info"
        centralModuleLoggers['core'] = "trace"

        centralBrokerLoggers['bbdo'] = "info"
        centralBrokerLoggers['core'] = "trace"

        const centralModuleMaster = config_module['centreonBroker']['output'].find((
              output => output.name === 'central-module-master-output'))
const centralBrokerMaster = config_broker['centreonBroker']['input'].find((
              input => input.name === 'central-broker-master-input'))

	
        for (let i = 0, k = 0; i < 3; ++i, k = i*3) {
          for (let j = 0; j < 3; ++j) { 
            Broker.clearLogs()
            Broker.clearLogsCentralModule()
            centralBrokerMaster['compression'] = compression[i] 
            centralModuleMaster['compression'] = compression[j] 

            await Broker.writeConfigCentralModule(config_module)
            await Broker.writeConfig(config_broker)

            console.log(centralBrokerMaster)
            console.log(centralModuleMaster)

            expect(await broker.start()).toBeTruthy()
            expect(await engine.start()).toBeTruthy()

            expect(await isBrokerAndEngineConnected()).toBeTruthy()

            console.log("index = ", k+j)
            expect(await Broker.checkLogFileContains([errors[k+j][0]])).toBeTruthy()
            expect(await Broker.checkLogFileCentralModuleContains([errors[k+j][1]])).toBeTruthy()

            expect(await broker.stop()).toBeTruthy();
            expect(await engine.stop()).toBeTruthy();
          }
        }
    }, 400000);
});
