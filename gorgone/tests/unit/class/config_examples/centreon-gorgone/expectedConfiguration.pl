return {
    'configuration' => {
        'centreon' => {
            'database' => {
                'db_configuration' => {
                    'dsn'      => 'mysql:host=localhost:port=3306;dbname=centreon',
                    'password' => 'VaultSentASecret',
                    'username' => 'centreon'
                },
                'db_realtime'      => {
                    'dsn'      => 'mysql:host=localhost:port=3306;dbname=centreon_storage',
                    'password' => 'password',
                    'username' => 'centreon'
                }
            }
        },
        'gorgone'  => {
            'tpapi'       => [
                {
                    'password' => 'VaultSentASecret',
                    'base_url' => 'http://127.0.0.1/centreon/api/latest/',
                    'name'     => 'centreonv2',
                    'username' => 'centreon-gorgone'
                },
                {
                    'username' => 'centreon-gorgone',
                    'name'     => 'clapi',
                    'password' => 'webapiPassword!'
                }
            ],
            'gorgonecore' => {
                'id'      => 1,
                'privkey' => '/var/lib/centreon-gorgone/.keys/rsakey.priv.pem',
                'pubkey'  => '/var/lib/centreon-gorgone/.keys/rsakey.pub.pem'
            },
            'modules'     => [
                {
                    'package'         => 'gorgone::modules::core::action::hooks',
                    'whitelist_cmds'  => 'VaultSentASecret',
                    'command_timeout' => 30,
                    'allowed_cmds'    => [
                        '^sudo\\s+(/bin/|/usr/bin/)?systemctl\\s+(reload|restart)\\s+(centengine|centreontrapd|cbd)\\s*$',
                        '^(sudo\\s+)?(/usr/bin/)?service\\s+(centengine|centreontrapd|cbd|cbd-sql)\\s+(reload|restart)\\s*$',
                        '^/usr/sbin/centenginestats\\s+-c\\s+/etc/centreon-engine/+centengine\\.cfg\\s*$',
                        '^cat\\s+/var/lib/centreon-engine/+[a-zA-Z0-9\\-]+-stats\\.json\\s*$',
                        '^/usr/lib/centreon/plugins/.*$',
                        '^/bin/perl /usr/share/centreon/bin/anomaly_detection --seasonality >> /var/log/centreon/anomaly_detection\\.log 2>&1\\s*$',
                        '^/usr/bin/php -q /usr/share/centreon/cron/centreon-helios\\.php >> /var/log/centreon-helios\\.log 2>&1\\s*$',
                        '^centreon',
                        '^mkdir',
                        '^/usr/share/centreon/www/modules/centreon-autodiscovery-server/script/run_save_discovered_host',
                        '^/usr/share/centreon/bin/centreon -u \\"centreon-gorgone\\" -p \\S+ -w -o CentreonWorker -a processQueue$',
                        '^/usr/bin/php (-q )?/usr/share/centreon/cron/[\\w,\\s.-]+ >> /var/log/centreon-gorgone/[\\w,\\s.-]+\\s+2>&1$',
                        '^/usr/bin/php -q /usr/share/centreon/www/modules/centreon-bi-server/tools/purgeArchivesFiles\\.php >> /var/log/centreon-gorgone/centreon-bi-archive-retention\\.log 2>&1$',
                        '^/usr/share/centreon/cron/eventReportBuilder --config=/etc/centreon/conf\\.pm >> /var/log/centreon-gorgone/eventReportBuilder\\.log 2>&1$',
                        '^/usr/share/centreon/cron/dashboardBuilder --config=/etc/centreon/conf\\.pm >> /var/log/centreon-gorgone/dashboardBuilder\\.log 2>&1$',
                        '^/usr/share/centreon/www/modules/centreon-dsm/+cron/centreon_dsm_purge\\.pl --config=\\"/etc/centreon/conf.pm\\" --severity=\\S+ >> /var/log/centreon-gorgone/centreon_dsm_purge\\.log 2>&1\\s*$',
                        '^/usr/share/centreon-bi-backup/centreon-bi-backup-web\\.sh >> /var/log/centreon-gorgone/centreon-bi-backup-web\\.log 2>&1$',
                        '^/usr/share/centreon/www/modules/centreon-autodiscovery-server/+cron/centreon_autodisco.pl --config=\'/etc/centreon/conf.pm\' --config-extra=\'/etc/centreon/centreon_autodisco.pm\' --severity=\\S+ >> /var/log/centreon-gorgone/centreon_service_discovery.log 2>&1$',
                        'VaultSentASecret'
                    ],
                    'name'            => 'action',
                    'enable'          => 'true'
                },
                {
                    'enable'        => 'true',
                    'ssl_cert_file' => '/var/lib/centreon-gorgone/.keys/server_api_cert.pem',
                    'ssl'           => 'true',
                    'auth'          => {
                        'user'     => 'web-user-gorgone-api',
                        'enabled'  => 'false',
                        'password' => 'password'
                    },
                    'name'          => 'httpserver',
                    'address'       => '0.0.0.0',
                    'allowed_hosts' => {
                        'enabled' => 'true',
                        'subnets' => [
                            '127.0.0.1/32'
                        ]
                    },
                    'port'          => '8085',
                    'package'       => 'gorgone::modules::core::httpserver::hooks',
                    'ssl_key_file'  => '/var/lib/centreon-gorgone/.keys/server_api_key.pem'
                },
                {
                    'enable'  => 'true',
                    'cron'    => [
                        {
                            'action'   => 'LAUNCHSERVICEDISCOVERY',
                            'timespec' => '30 22 * * *',
                            'id'       => 'service_discovery'
                        }
                    ],
                    'name'    => 'cron',
                    'package' => 'gorgone::modules::core::cron::hooks'
                },
                {
                    'package' => 'gorgone::modules::core::register::hooks',
                    'name'    => 'register',
                    'enable'  => 'true'
                },
                {
                    'enable'  => 'true',
                    'package' => 'gorgone::modules::centreon::nodes::hooks',
                    'name'    => 'nodes'
                },
                {
                    'enable'      => 'true',
                    'httpserver'  => {
                        'enable'  => 'true',
                        'port'    => 8099,
                        'token'   => "^\$*\x{f9}^\x{e9}&\x{e0}\x{e9}r\x{e7}(\x{e9}/*-+\$\$z\@ze%r\x{a8}\x{a3}\x{b5}~zz",
                        'address' => '0.0.0.0'
                    },
                    'package'     => 'gorgone::modules::core::proxy::hooks',
                    'name'        => 'proxy',
                    'pool'        => 1,
                    'buffer_size' => 10
                },
                {
                    'cmd_dir'        => '/var/lib/centreon/centcore/',
                    'buffer_size'    => 100,
                    'cache_dir'      => '/var/cache/centreon/',
                    'enable'         => 'true',
                    'name'           => 'legacycmd',
                    'remote_dir'     => '/var/cache/centreon//config/remote-data/',
                    'cmd_file'       => '/var/lib/centreon/centcore.cmd',
                    'cache_dir_trap' => '/etc/snmp/centreon_traps',
                    'package'        => 'gorgone::modules::centreon::legacycmd::hooks'
                },
                {
                    'enable'       => 'true',
                    'command_file' => '/var/lib/centreon-engine/rw/centengine.cmd',
                    'name'         => 'engine',
                    'package'      => 'gorgone::modules::centreon::engine::hooks'
                },
                {
                    'name'             => 'statistics',
                    'package'          => 'gorgone::modules::centreon::statistics::hooks',
                    'enable'           => 'true',
                    'cron'             => [
                        {
                            'action'     => 'BROKERSTATS',
                            'timespec'   => '*/5 * * * *',
                            'id'         => 'broker_stats',
                            'parameters' => {
                                'timeout' => 10
                            }
                        },
                        {
                            'action'     => 'ENGINESTATS',
                            'id'         => 'engine_stats',
                            'timespec'   => '*/5 * * * *',
                            'parameters' => {
                                'timeout' => 10
                            }
                        }
                    ],
                    'broker_cache_dir' => '/var/cache/centreon//broker-stats/'
                },
                {
                    'enable'  => 'true',
                    'package' => 'gorgone::modules::centreon::autodiscovery::hooks',
                    'name'    => 'autodiscovery'
                },
                {
                    'package' => 'gorgone::modules::centreon::audit::hooks',
                    'name'    => 'audit',
                    'enable'  => 'true'
                }
            ]
        }
    }
};
