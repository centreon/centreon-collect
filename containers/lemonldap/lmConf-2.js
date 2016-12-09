{
    "captchaStorageOptions": {},
    "samlSPMetaDataXML": null,
    "domain": "centreon.com",
    "managerDn": "cn=admin,dc=centreon,dc=com",
    "globalStorage": "Apache::Session::File",
    "CAS_proxiedServices": {},
    "cfgAuthorIP": "10.40.1.188",
    "vhostOptions": {
        "centreon.centreon.com": {},
        "manager.centreon.com": {
            "vhostMaintenance": 0,
            "vhostHttps": -1
        }
    },
    "exportedVars": {
        "UA": "HTTP_USER_AGENT"
    },
    "ldapBase": "dc=centreon,dc=com",
    "oidcServiceMetaDataAuthnContext": {},
    "exportedHeaders": {
        "centreon.centreon.com": {
            "Auth-User": "$uid"
        },
        "manager.centreon.com": {}
    },
    "cfgDate": 1481278558,
    "ldapExportedVars": {},
    "sessionDataToRemember": {},
    "issuerDBGetParameters": {},
    "facebookExportedVars": {},
    "samlSPMetaDataOptions": null,
    "post": {
        "centreon.centreon.com": {},
        "manager.centreon.com": {}
    },
    "ldapVersion": 3,
    "casAttributes": {},
    "registerDB": "Demo",
    "authChoiceModules": {},
    "oidcOPMetaDataJSON": null,
    "passwordDB": "LDAP",
    "remoteGlobalStorageOptions": {},
    "cookieName": "lemonldap",
    "casStorageOptions": {},
    "macros": {
        "_whatToTrace": "$_auth eq 'SAML' ? \"$_user\\@$_idpConfKey\" : \"$_user\""
    },
    "reloadUrls": {
        "reload.centreon.com": "http://reload.centreon.com/reload"
    },
    "samlIDPMetaDataOptions": null,
    "oidcOPMetaDataJWKS": null,
    "userDB": "LDAP",
    "notificationStorage": "File",
    "googleExportedVars": {},
    "timeout": 72000,
    "localSessionStorage": "Cache::FileCache",
    "AuthLDAPFilter":"(&(uid=$user)(objectClass=posixAccount))",
    "mailLDAPFilter":"(&(mail=$mail)(objectClass=posixAccount))",
    "registerUrl": "http://auth.centreon.com/register.pl",
    "oidcStorageOptions": {},
    "grantSessionRules": {},
    "notificationStorageOptions": {
        "dirName": "/var/lib/lemonldap-ng/notifications"
    },
    "portalSkinRules": {},
    "mailUrl": "http://auth.centreon.com/mail.pl",
    "slaveExportedVars": {},
    "logoutServices": {},
    "securedCookie": 0,
    "samlStorageOptions": {},
    "portalSkinBackground": "1280px-Cedar_Breaks_National_Monument_partially.jpg",
    "lwpSslOpts": {},
    "persistentStorage": "Apache::Session::File",
    "dbiExportedVars": {},
    "ldapPort": 389,
    "globalStorageOptions": {
        "generateModule": "Lemonldap::NG::Common::Apache::Session::Generate::SHA256",
        "Directory": "/var/lib/lemonldap-ng/sessions",
        "LockDirectory": "/var/lib/lemonldap-ng/sessions/lock"
    },
    "oidcRPMetaDataOptions": null,
    "whatToTrace": "_whatToTrace",
    "ldapServer": "ldap://openldap",
    "groups": {},
    "cfgNum": 5,
    "loginHistoryEnabled": 1,
    "demoExportedVars": {
        "uid": "uid",
        "mail": "mail",
        "cn": "cn"
    },
    "persistentStorageOptions": {
        "Directory": "/var/lib/lemonldap-ng/psessions",
        "LockDirectory": "/var/lib/lemonldap-ng/psessions/lock"
    },
    "oidcOPMetaDataOptions": null,
    "managerPassword": "centreon",
    "localSessionStorageOptions": {
        "default_expires_in": 600,
        "namespace": "lemonldap-ng-sessions",
        "directory_umask": "007",
        "cache_depth": 3,
        "cache_root": "/tmp"
    },
    "ldapAuthnLevel": 2,
    "openIdExportedVars": {},
    "authentication": "LDAP",
    "notification": 1,
    "samlIDPMetaDataXML": null,
    "ldapTimeout": 120,
    "oidcRPMetaDataExportedVars": null,
    "locationRules": {
        "manager.centreon.com": {
            "(?#Configuration)^/(manager\\.html|conf/)": "$uid eq \"dwho\"",
            "(?#Notifications)/notifications": "$uid eq \"dwho\" or $uid eq \"rtyler\"",
            "(?#Sessions)/sessions": "$uid eq \"dwho\" or $uid eq \"rtyler\"",
            "default": "$uid eq \"dwho\""
        },
        "centreon.centreon.com": {
            "default": "accept",
            "^/logout": "logout_sso"
        }
    },
    "applicationList": {
        "1sample": {
            "test1": {
                "options": {
                    "logo": "demo.png",
                    "name": "Application Test 1",
                    "description": "A simple application displaying authenticated user",
                    "uri": "http://test1.centreon.com/",
                    "display": "auto"
                },
                "type": "application"
            },
            "type": "category",
            "test2": {
                "type": "application",
                "options": {
                    "display": "auto",
                    "uri": "http://test2.centreon.com/",
                    "description": "The same simple application displaying authenticated user",
                    "name": "Application Test 2",
                    "logo": "thumbnail.png"
                }
            },
            "catname": "Sample applications"
        },
        "2administration": {
            "notifications": {
                "type": "application",
                "options": {
                    "logo": "database.png",
                    "description": "Explore WebSSO notifications",
                    "name": "Notifications explorer",
                    "uri": "http://manager.centreon.com/notifications.html",
                    "display": "auto"
                }
            },
            "type": "category",
            "catname": "Administration",
            "manager": {
                "type": "application",
                "options": {
                    "name": "WebSSO Manager",
                    "description": "Configure LemonLDAP::NG WebSSO",
                    "uri": "http://manager.centreon.com/manager.html",
                    "display": "auto",
                    "logo": "configure.png"
                }
            },
            "sessions": {
                "type": "application",
                "options": {
                    "description": "Explore WebSSO sessions",
                    "name": "Sessions explorer",
                    "display": "auto",
                    "uri": "http://manager.centreon.com/sessions.html",
                    "logo": "database.png"
                }
            }
        },
        "3documentation": {
            "localdoc": {
                "type": "application",
                "options": {
                    "display": "on",
                    "uri": "http://manager.centreon.com/doc/",
                    "name": "Local documentation",
                    "description": "Documentation supplied with LemonLDAP::NG",
                    "logo": "help.png"
                }
            },
            "type": "category",
            "officialwebsite": {
                "options": {
                    "logo": "network.png",
                    "uri": "http://lemonldap-ng.org/",
                    "display": "on",
                    "description": "Official LemonLDAP::NG Website",
                    "name": "Offical Website"
                },
                "type": "application"
            },
            "catname": "Documentation"
        }
    },
    "samlIDPMetaDataExportedAttributes": null,
    "key": "p7J7d7w\"z=laOo'h",
    "webIDExportedVars": {},
    "samlSPMetaDataExportedAttributes": null,
    "oidcOPMetaDataExportedVars": null,
    "portal": "http://auth.centreon.com/",
    "portalSkin": "bootstrap",
    "cfgLog": "",
    "cfgAuthor": "dwho"
}