import sleep from 'await-sleep';
import shell from 'shelljs';
import { once } from 'events'
import { Broker } from '../core/broker';
import { Engine } from '../core/engine';
import { isBrokerAndEngineConnected } from '../core/brokerEngine';
import { broker } from 'shared';

const process = require('process')

// shell.config.silent = true;

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

      const tls = ["yes", "no", "auto"]
      const errors = [
        // yes, yes
        [
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'",
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'"
        ],
        // yes, no 
        [
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has ''",
          "[bbdo] [info] BBDO: we have extensions '' and peer has 'TLS'"
        ],
        // yes, auto
        [
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'",
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'"
        ],
        // no, yes
        [
          "[bbdo] [info] BBDO: we have extensions '' and peer has 'TLS'",
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has ''"
        ],
        // no, no
        [
          "[bbdo] [info] BBDO: we have extensions \'\' and peer has \'\'",
          "[bbdo] [info] BBDO: we have extensions \'\' and peer has \'\'"
        ],

        // no, auto 
        [
          "[bbdo] [info] BBDO: we have extensions '' and peer has 'TLS'",
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has ''"
        ],
        // auto, yes 
        [
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'",
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'"
        ],
        // auto, no 
        [
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has ''",
          "[bbdo] [info] BBDO: we have extensions '' and peer has 'TLS'"
        ],
        // auto, auto 
        [
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'",
          "[bbdo] [info] BBDO: we have extensions 'TLS' and peer has 'TLS'"
        ]
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
            centralBrokerMaster['tls'] = tls[i] 
            centralModuleMaster['tls'] = tls[j] 

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

    it.only('tls with keys checks between broker - engine', async () => {
      const broker = new Broker()
      const engine = new Engine()

      const config_broker = await Broker.getConfig()
      const config_rrd = await Broker.getConfigCentralRrd()

      const centralBrokerLoggers = config_broker['centreonBroker']['log']['loggers']

      centralBrokerLoggers['bbdo'] = "info"
      centralBrokerLoggers['tcp']  = "info"
      centralBrokerLoggers['tls']  = "trace"

      var centralBrokerMaster = config_broker['centreonBroker']['output'].find((
              output => output.name === 'centreon-broker-master-rrd'))
      const centralRrdMaster = config_rrd['centreonBroker']['input'].find((
              input => input.name === 'central-rrd-master-input'))
      
      // get hostname
      const output = await shell.exec("hostname --fqdn")
      const hostname = output.stdout.replace(/\n/g, '')
      
      // generates keys
      await shell.exec("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/ssl/server.key -out /etc/centreon-broker/ssl/server.crt -subj '/CN='" + hostname)
      await shell.exec("openssl req -new -newkey rsa:2048 -days 365 -nodes -x509 -keyout /etc/centreon-broker/ssl/client.key -out /etc/centreon-broker/ssl/client.crt -subj '/CN='" + hostname)
      
      // update configuration file
      centralBrokerMaster["tls"] = "yes"
      centralBrokerMaster["private_key"] = "/etc/centreon-broker/ssl/client.key"
      centralBrokerMaster["public_cert"] = "/etc/centreon-broker/ssl/client.crt"
      centralBrokerMaster["ca_certificate"] = "/etc/centreon-broker/ssl/server.crt"

      centralRrdMaster["tls"] = "yes"
      centralRrdMaster["private_key"] = "/etc/centreon-broker/ssl/server.key"
      centralRrdMaster["public_cert"] = "/etc/centreon-broker/ssl/server.crt"
      centralRrdMaster["ca_certificate"] = "/etc/centreon-broker/ssl/client.crt"
      
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

      await shell.rm("/etc/centreon-broker/ssl/client.key")
      await shell.rm("/etc/centreon-broker/ssl/client.crt")
      await shell.rm("/etc/centreon-broker/ssl/server.key")
      await shell.rm("/etc/centreon-broker/ssl/server.crt")
    }, 90000);
});
