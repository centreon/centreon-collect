module.exports = {
  // Database configuration
  database: {
    host: 'localhost',
    user: 'root',
    password: 'centreon',
    database: 'imp',
    debug: false
  },
  cache: {
     // The redis server
    host: 'localhost'
  },
  ldap: {
    // For more options see http://ldapjs.org/client.html
    connect: {
      // The ldap url in format ldap://ip:port
      url: 'ldap://localhost:389',
      // Milliseconds after last activity before client emits idle event
      idleTimeout: 360000
    },
    config: {
      // The user / pass for connect to ldap server
      binddn: 'admin',
      password: 'centreon',
      // The filter for find users, the macro %username% will be replace by the
      // login
      authFilter: '(uid=%username%)',
      // The base dn for find users
      authBase: ''
    }
  },
  // The api full base url example : http://localhost:3000/api
  baseUrl: 'http://localhost:3000/api',
  // Configure logger
  logger: {
    // Logger transport type Console or ElasticSearch
    type: 'Console',
    options: {
      host: '',
      // Level
      level: 'debug',
      indexPrefix: 'centreon-imp-'
    }
  },
  // Information for sign license
  license: {
    key: {
      // The path to the private key file
      path: 'private.asc',
      // The password for load the private key file
      password: 'centreon'
    }
  },
  // Information for kayako
  supportPlatform: {
    url: 'localhost',
    apiKey: '',
    secretKey: '',
    organizationId: 1,
    departmantId: 1
  }
};
