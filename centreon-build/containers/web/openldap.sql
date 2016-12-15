--
-- Contenu de la table contact (contact template)
--
INSERT INTO `contact` (`contact_name`, `contact_alias`, `contact_activate`, `contact_auth_type`, `contact_register`)
VALUES ('contact_template', 'contact_template', '1', 'local', 0);

--
-- Contenu de la table auth_ressource
--
INSERT INTO `auth_ressource` (`ar_name`, `ar_description`, `ar_type`, `ar_enable`)
VALUES ('openldap', 'openldap', 'ldap', '1');

--
-- Contenu de la table auth_ressource_info
--
INSERT INTO `auth_ressource_info` (`ar_id`, `ari_name`, `ari_value`)
VALUES
(1, 'alias', 'uid'),
(1, 'bind_dn', 'cn=admin,dc=centreon,dc=com'),
(1, 'bind_pass', 'centreon'),
(1, 'group_base_search', 'dc=centreon,dc=com'),
(1, 'group_filter', '(&(cn=%s)(objectClass=groupOfNames))'),
(1, 'group_member', 'member'),
(1, 'group_name', 'cn'),
(1, 'ldap_auto_import', '1'),
(1, 'ldap_contact_tmpl', (SELECT contact_id from contact where contact_name = 'contact_template')),
(1, 'ldap_dns_use_domain', ''),
(1, 'ldap_dns_use_ssl', '0'),
(1, 'ldap_dns_use_tls', '0'),
(1, 'ldap_search_limit', '60'),
(1, 'ldap_search_timeout', '60'),
(1, 'ldap_srv_dns', '0'),
(1, 'ldap_store_password', '1'),
(1, 'ldap_template', 'Posix'),
(1, 'protocol_version', '3'),
(1, 'user_base_search', 'dc=centreon,dc=com'),
(1, 'user_email', 'mail'),
(1, 'user_filter', '(&(uid=%s)(objectClass=posixAccount))'),
(1, 'user_firstname', 'givenname'),
(1, 'user_group', ''),
(1, 'user_lastname', 'sn'),
(1, 'user_name', 'cn'),
(1, 'user_pager', 'mobile');

--
-- Contenu de la table auth_ressource_host
--
INSERT INTO `auth_ressource_host` (`auth_ressource_id`, `host_address`, `host_port`, `use_ssl`, `use_tls`, `host_order`)
VALUES (1, 'openldap', 389, 0, 0, 1);

--
-- Contenu de la table contact (user)
--
INSERT INTO `contact` (`contact_name`, `contact_alias`, `contact_lang`, `contact_activate`, `contact_oreon`, `contact_template_id`, `contact_admin`, `contact_auth_type`, `contact_ldap_dn`, `ar_id`, `contact_register`)
VALUES ('centreon-ldap', 'centreon-ldap', 'en_US', '1', '1', (SELECT c2.contact_id FROM contact c2 WHERE c2.contact_name = 'contact_template'), '1', 'ldap', 'cn=centreon-ldap,ou=users,dc=centreon,dc=com', 1, 1);