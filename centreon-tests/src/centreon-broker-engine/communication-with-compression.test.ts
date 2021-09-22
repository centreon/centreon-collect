import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';
import { isBrokerAndEngineConnected } from '../core/brokerEngine';
import { broker } from 'shared';


shell.config.silent = true;

describe('engine and broker testing in same time for compression', () => {

    beforeEach(() => {
        Broker.cleanAllInstances();
        Engine.cleanAllInstances();

        Broker.clearLogs()
        Broker.clearLogsCentralModule()
        Broker.resetConfig()
        Broker.resetConfigCentralModule()

        if (Broker.isServiceRunning() || Engine.isServiceRunning()) {
          console.log("program could not stop cbd or centengine")
          process.exit(1)
        }

    })

    afterAll(() => {
        beforeEach(() => {
            Broker.cleanAllInstances();
            Engine.cleanAllInstances();

            Broker.clearLogs()
            Broker.resetConfig()
            Broker.resetConfigCentralModule()
        })
    })

    it('compression checks between broker - engine', async () => {
        const broker = new Broker()
        const engine = new Engine()

        let compression = {
            yes: 'COMPRESSION',
            no: '',
            auto: 'COMPRESSION'
        }

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

        for (let c1 in compression) {
            for (let c2 in compression) {
                Broker.clearLogs()
                Broker.clearLogsCentralModule()

                centralBrokerMaster['compression'] = c1
                centralModuleMaster['compression'] = c2

                let peer1 = [`[bbdo] [info] BBDO: we have extensions '${compression[c1]}' and peer has '${compression[c2]}'`];
                let peer2 = [`[bbdo] [info] BBDO: we have extensions '${compression[c2]}' and peer has '${compression[c1]}'`];

                if (c1 == 'yes' && c2 == 'no')
                    peer1.push("[bbdo] [error] BBDO: extension 'COMPRESSION' is set to 'yes' in the configuration but cannot be activated because of peer configuration.");
                else if (c1 == 'no' && c2 == 'yes')
                    peer2.push("[bbdo] [error] BBDO: extension 'COMPRESSION' is set to 'yes' in the configuration but cannot be activated because of peer configuration.");

                console.log(centralBrokerMaster)
                console.log(centralModuleMaster)

                await Broker.writeConfigCentralModule(config_module)
                await Broker.writeConfig(config_broker)

                await expect(broker.start()).resolves.toBeTruthy()
                await expect(engine.start()).resolves.toBeTruthy()

                await expect(isBrokerAndEngineConnected()).resolves.toBeTruthy()

                await expect(Broker.checkLogFileContains(peer1)).resolves.toBeTruthy();
                await expect(Broker.checkLogFileCentralModuleContains(peer2)).resolves.toBeTruthy()

                await expect(broker.stop()).resolves.toBeTruthy();
                await expect(engine.stop()).resolves.toBeTruthy();
            }
        }

    }, 400000);

});
