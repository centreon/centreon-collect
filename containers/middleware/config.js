module.exports = {
  // Database configuration
  database: {
    host: 'localhost',
    user: 'root',
    password: '',
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
      binddn: 'cn=Manager,dn=centreon,dn=com',
      password: 'centreon',
      // The filter for find users, the macro %username% will be replace by the
      // login
      authFilter: '(uid=%username%)',
      // The base dn for find users
      authBase: 'dn=centreon,dn=com'
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
  }
};
