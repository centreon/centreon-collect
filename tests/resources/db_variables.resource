*** Variables ***
${EtcRoot}          /tmp/etc
${VarRoot}          /tmp/var
${DBName}           centreon_storage
${DBNameConf}       centreon
${DBHost}           localhost
${DBUser}           centreon
${DBPass}           centreon
${DBPort}           3306
${DBUserRoot}       root_centreon
${DBPassRoot}       centreon
${DBSslEnabled}     %{DB_SSL_ENABLED=}
${DBSslCa}          %{DB_SSL_CA=}
${DBSslCert}        %{DB_SSL_CERT=}
${DBSslKey}         %{DB_SSL_KEY=}
${DBTlsVersion}     %{DB_TLS_VERSION=TLSv1.3}
