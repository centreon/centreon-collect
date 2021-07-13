import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';
import { isBrokerAndEngineConnected } from '../core/brokerEngine';
import { broker } from 'shared';

import util from 'util';
import process from 'process';

shell.config.silent = true;

describe('engine and broker testing in same time for compression', () => {
    beforeEach(() => {
        Broker.clearLogs()
        Broker.clearLogsCentralModule()
        Broker.resetConfig()
        Broker.resetConfigCentralModule()
        Broker.resetConfigCentralRrd()
    })

    afterAll(() => {
        beforeEach(() => {
            Broker.clearLogs()
            Broker.resetConfig()
            Broker.resetConfigCentralModule()
            Broker.resetConfigCentralRrd()
        })
    })

    it('tls without keys checks between broker - engine', async () => {
        const broker = new Broker()
        const engine = new Engine()

        let tls = {
            yes: 'TLS',
            no: '',
            auto: 'TLS'
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

        for (let t1 in tls) {
            for (let t2 in tls) {
                Broker.clearLogs()
                Broker.clearLogsCentralModule()

                centralBrokerMaster['tls'] = t1
                centralModuleMaster['tls'] = t2

                let peer1 = `[bbdo] [info] BBDO: we have extensions '${tls[t1]}' and peer has '${tls[t2]}'`
                let peer2 = `[bbdo] [info] BBDO: we have extensions '${tls[t2]}' and peer has '${tls[t1]}'`
                console.log(centralBrokerMaster)
                console.log(centralModuleMaster)

                await Broker.writeConfigCentralModule(config_module)
                await Broker.writeConfig(config_broker)

                expect(await broker.start()).toBeTruthy()
                expect(await engine.start()).toBeTruthy()

                expect(await isBrokerAndEngineConnected()).toBeTruthy()

                expect(await Broker.checkLogFileContains([peer1])).toBeTruthy()
                expect(await Broker.checkLogFileCentralModuleContains([peer2])).toBeTruthy()

                expect(await broker.stop()).toBeTruthy();
                expect(await engine.stop()).toBeTruthy();
            }
        }
    }, 400000);

    it.only('tls with keys checks between broker - engine', async () => {
        const broker = new Broker()
        const engine = new Engine()

        const config_broker = await Broker.getConfig()
        const config_rrd = await Broker.getConfigCentralRrd()

        const centralBrokerLoggers = config_broker['centreonBroker']['log']['loggers']

        centralBrokerLoggers['bbdo'] = "info"
        centralBrokerLoggers['tcp'] = "info"
        centralBrokerLoggers['tls'] = "trace"

        var centralBrokerMaster = config_broker['centreonBroker']['output'].find((
            output => output.name === 'centreon-broker-master-rrd'))
        const centralRrdMaster = config_rrd['centreonBroker']['input'].find((
            input => input.name === 'central-rrd-master-input'))

        // get hostname
        const output = await shell.exec("hostname --fqdn")
        const hostname = output.stdout.replace(/\n/g, '')

        // generates keys
        await shell.exec("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/server.key -out /etc/centreon-broker/server.crt -subj '/CN='" + hostname)
        await shell.exec("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/client.key -out /etc/centreon-broker/client.crt -subj '/CN='" + hostname)

        // update configuration file
        centralBrokerMaster["tls"] = "yes"
        centralBrokerMaster["private_key"] = "/etc/centreon-broker/client.key"
        centralBrokerMaster["public_cert"] = "/etc/centreon-broker/client.crt"
        centralBrokerMaster["ca_certificate"] = "/etc/centreon-broker/server.crt"

        centralRrdMaster["tls"] = "yes"
        centralRrdMaster["private_key"] = "/etc/centreon-broker/server.key"
        centralRrdMaster["public_cert"] = "/etc/centreon-broker/server.crt"
        centralRrdMaster["ca_certificate"] = "/etc/centreon-broker/client.crt"

        // write changes in config_broker
        await Broker.writeConfig(config_broker)
        await Broker.writeConfigCentralRrd(config_rrd)

        console.log(centralBrokerMaster)
        console.log(centralRrdMaster)

        // starts centreon
        expect(await broker.start()).toBeTruthy()
        expect(await engine.start()).toBeTruthy()

        expect(await isBrokerAndEngineConnected()).toBeTruthy()

        // checking logs 
        expect(await Broker.checkLogFileContains(["[tls] [info] TLS: using certificates as credentials"])).toBeTruthy()
        expect(await Broker.checkLogFileContains(["[tls] [debug] TLS: performing handshake"])).toBeTruthy()
        expect(await Broker.checkLogFileContains(["[tls] [debug] TLS: successful handshake"])).toBeTruthy()

        expect(await broker.stop()).toBeTruthy();
        expect(await engine.stop()).toBeTruthy();

        await shell.rm("/etc/centreon-broker/client.key")
        await shell.rm("/etc/centreon-broker/client.crt")
        await shell.rm("/etc/centreon-broker/server.key")
        await shell.rm("/etc/centreon-broker/server.crt")
    }, 90000);
});
