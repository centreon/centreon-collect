# Centreon Build : Containers : lemonldap

## Introduction

LemonLDAP::NG is a modular WebSSO (Single Sign On) based on Apache::Session modules.
It simplifies the build of a protected area with a few changes in the application.

It manages both authentication and authorization and provides headers for accounting.

## How to run lemonldap

You need to modify docker-compose file
(*centreon-build/containers/lemonldap/docker-compose.yml.in*)
and replace "@WEB_IMAGE@" by Centreon Web image you want to use
(i.e "ci.int.centreon.com:5000/mon-web:centos7")

Then, you can use docker-compose to run lemonldap SSO architecture :<br>
`docker-compose -f centreon-build/containers/lemonldap/docker-compose.yml.in up -d`

To avoid to use a firewall to redirect requests to the reverse proxy,
you need to modify your **hosts** file:
  * Linux : /etc/hosts
  * Windows : C:\Windows\System32\drivers\etc\hosts

Then add the following by replacing <ip_address> by the ip address of your server :<br>
`<ip_address> centreon.centreon.com manager.centreon.com handler.centreon.com auth.centreon.com`

## How to use SSO

Some addresses are available :
  * **centreon.centreon.com** : Centreon Web
  * **auth.centreon.com** : SSO authentication portal
  * **manager.centreon.com** : SSO management website

SSO must be enabled in Centreon Web.<br>
If you use **Mixed mode**, trusted client address is mandatory
(it is your lemonldap ip address).