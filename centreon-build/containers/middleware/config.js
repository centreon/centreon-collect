module.exports = {
  // Database configuration
  database: {
    host: 'localhost',
    user: 'root',
    password: 'centreon',
    database: 'imp',
    debug: false,
    connectTimeout: 30000
  },
  cache: {
     // The redis server
    host: 'redis'
  },
  ldap: {
    // For more options see http://ldapjs.org/client.html
    connect: {
      // The ldap url in format ldap://ip:port
      url: 'ldap://openldap:389',
      // Milliseconds after last activity before client emits idle event
      idleTimeout: 360000
    },
    config: {
      // The user / pass for connect to ldap server
      binddn: 'cn=admin,dc=centreon,dc=com',
      password: 'centreon',
      // The filter for find users, the macro %username% will be replace by the
      // login
      authFilter: '(&(uid=%username%)(memberOf=cn=auth-middleware,ou=groups,dc=centreon,dc=com))',
      // The base dn for find users
      authBase: 'ou=users,dc=centreon,dc=com'
    }
  },
  // The api full base url example : http://localhost:3000/api
  baseUrl: 'http://localhost:3000/api',
  // Configure logger
  logger: {
    // Logger transport type Console or ElasticSearch
    transports: {
      defaultTransports: {
        type: 'Console',
        level: 'error'
      },
      httpTransports: {
        type: 'Console',
        level: 'debug'
      },
      appTransports: {
        type: 'Console',
        level: 'debug'
      }
    },
    loggers: {
      default: ['defaultTransports'],
      httpLogs: ['httpTransports'],
      appLogs: ['appTransports']
    }
  },
  // Information for sign license
  license: {
    key: {
      // The path to the private key file
      path: 'private.asc',
      // The password for load the private key file
      password: ''
    }
  },
  // Information for kayako
  supportPlatform: {
    url: 'http://support.centreon.com/api/',
    apiKey: '4e942cec-6e77-3f44-110c-9848a9ffd8a1',
    secretKey: 'ZjFlZGVhZmUtMTE5NC1iY2I0LTk1MjgtM2IwYmJmZDMzYzIwNGFlYmFmNjUtMmY1Yi04MWE0LWU5NWUtZWZlYTYwNmMyMDYy',
    organizationId: 104,
    departmantId: 363
  },
  // Information for sugar crm
  crmPlatform: {
    url: 'http://crm.int.centreon.com/rest/v10/',
    clientId: 'middleware',
    clientSecret: 'secret',
    username: 'dp.dev',
    password: 'centreon'
  },
  // Information for saleforce
  salesforce: {
    test: true,
    username: 'apiuser@centreon.com.sandbox',
    password: '1234centreon!',
    security: 'fsrit36e0tJxcIvYUzizhqgO',
    ids: {
      IMP_1_MONTH: '01u0Y000001JstcQAC',
      IMP_12_MONTH: '01u0Y000001JstdQAC',
      IMP_6_MONTH: '01u0Y000001JsteQAC'
    }
  },
  // Intercom
  intercom: {
    region: 'eu-west-1',
    queue: 'http://sqs:9324/queue/default'
  },
  // Information for mailchimp
  newsletter: {
    apiKey: '8e54ec13a4494c269d4a1778fcd8b637-us2',
    listId: '1eeddd2b3a'
  },
  // APM Monitoring
  newrelic: {
    enable: false
  },
  // JWT Secure key
  jwtSecureKey: 'centreon'
};
