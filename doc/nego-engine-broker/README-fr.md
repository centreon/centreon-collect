Négociation entre Engine et Broker
==================================

# Introduction

Ce document décrit comment `Engine` et `Broker` coopèrent en BBDO 3 depuis l'arrivée de la **configuration centralisée**, et comment cette base prépare le **Poller HA**.

## D'où l'on part

À l'origine, PHP générait la configuration de chaque poller à partir de la base de configuration, et Gorgone la déposait sur le poller. À chaque démarrage, `Engine` lisait son `centengine.cfg` puis renvoyait toute sa configuration à `Broker`, qui s'en servait pour préparer la base temps réel. `Engine` était aussi le seul à détenir l'état d'exécution : état des checks, downtimes, commentaires, acquittements, notifications. Cet état était sauvegardé dans un `retention.dat` propre à chaque poller. Une ressource était donc liée à son poller, et le central ne savait rien qu'on ne lui ait d'abord envoyé.

## Ce qui existe aujourd'hui

Deux régimes coexistent. Le choix se fait par poller (voir [Vue d'ensemble : qui gère quoi](01-negotiation-fr.md#vue-densemble--qui-gère-quoi-selon-la-configuration)) :

* **legacy** : rien ne change par rapport à la situation historique ;
* **centralisé** (BBDO ≥ 3, `cache_config_directory`, `Engine` lancé avec `-p`). `Broker` possède la configuration des pollers :
  * PHP écrit la configuration de chaque poller dans un répertoire et l'annonce par un fichier `.lck` ;
  * `Broker` la lit, la valide, y compris les dépendances entre pollers, et la conserve sous forme sérialisée dans `<ID>.prot` ;
  * il calcule la différence et l'envoie à `Engine` sous forme de `DiffState` ;
  * `Engine` l'applique et en garde une copie dans `state.prot`.

  Si des pollers sont rattachés à un remote server, le `Broker` de ce dernier sert de relais et transmet ces échanges vers le central.

`Broker` est ainsi devenu le dépositaire de l'état. Son cache global (`broker_cache`), persisté sur disque, connaît toutes les ressources de tous les pollers. À la connexion d'`Engine`, `Broker` lui renvoie son état d'exécution (`RuntimeState`), qui remplace `retention.dat`. Avec `notification_mode = broker`, `Broker` possède en plus les downtimes, les acquittements, les commentaires et la décision de notification. Les commandes externes peuvent passer par `Broker`, qui les route vers le bon poller. BAM tourne dans `Broker` et se recharge quand la configuration change.

## Où l'on va

Tous ces travaux sont les prérequis du Poller HA. Le premier jalon est une redondance actif/passif à placement statique. `Broker` reste un point de défaillance unique pour ce jalon, c'est un choix assumé. L'architecture cible est décrite dans [Architecture cible — vers la HA des pollers](../ha-target-architecture-fr.md).

## Plan du document

| Chapitre | Contenu |
|---|---|
| [Nouvelle négociation](01-negotiation-fr.md#nouvelle-négociation) | `cbmod` en librairie, paramètres, échange `Welcome` |
| [Lecture de la configuration Engine](02-configuration-delivery-fr.md#lecture-de-la-configuration-engine) | annonce, préparation, livraison et acquittement, tour de configuration, surveillance du répertoire |
| [Validation et écriture de la configuration](03-configuration-validation-fr.md#validation-et-écriture-de-la-configuration) | validation inter-pollers, `CheckPollerConfig`, calcul de la différence, écriture en base |
| [Cache centralisé Broker](04-centralized-cache-fr.md#cache-centralisé-broker) | alimentation, persistance, mode legacy |
| [Rétention et stream RRD](05-retention-rrd-fr.md#rétention-et-stream-rrd) | reprise d'une longue rétention sans bloquer le temps réel |
| [Remote Servers](06-remote-servers-fr.md#remote-servers-et-configuration-centralisée) | relais, topologie, migration d'un poller entre remotes |
| [Downtimes et acquittements](07-downtimes-acknowledgements-fr.md#gestion-centralisée-des-downtimes-et-acquittements) | `notification_mode`, notification décidée par `Broker` |
| [BAM dans Broker](08-bam-fr.md#bam-dans-broker) | fonctionnement actuel dans les deux régimes |
| [Travaux préparatoires](09-ha-preparatory-work-fr.md#travaux-préparatoires-avant-le-poller-ha) | les cinq prérequis au HA, dont l'élimination de `retention.dat` |
| [Poller HA](10-poller-ha-fr.md#poller-ha) | zones, affectation, distribution, rééquilibrage |
| [Tickets et soucis](11-tickets-and-issues-fr.md#tickets) | découpage en tickets, soucis potentiels et leur résolution |

# Sommaire

<!-- TOC -->
* [Nouvelle négociation](01-negotiation-fr.md#nouvelle-négociation)
  * [`cbmod` devient une librairie](01-negotiation-fr.md#cbmod-devient-une-librairie)
  * [Paramètres de démarrage d'Engine](01-negotiation-fr.md#paramètres-de-démarrage-dengine)
  * [Paramètres de Broker](01-negotiation-fr.md#paramètres-de-broker)
  * [Vue d'ensemble : qui gère quoi, selon la configuration](01-negotiation-fr.md#vue-densemble--qui-gère-quoi-selon-la-configuration)
  * [La négociation](01-negotiation-fr.md#la-négociation)
* [Lecture de la configuration Engine](02-configuration-delivery-fr.md#lecture-de-la-configuration-engine)
  * [Gestion de la configuration côté Engine](02-configuration-delivery-fr.md#gestion-de-la-configuration-côté-engine)
  * [Gestion de l’envoi de la configuration par Broker à Engine](02-configuration-delivery-fr.md#gestion-de-lenvoi-de-la-configuration-par-broker-à-engine)
  * [Quelques points plus techniques](02-configuration-delivery-fr.md#quelques-points-plus-techniques)
  * [Split de broker::config::applier::state](02-configuration-delivery-fr.md#split-de-brokerconfigapplierstate)
* [Validation et écriture de la configuration](03-configuration-validation-fr.md#validation-et-écriture-de-la-configuration)
  * [La validation inter-pollers : `load_foreign_objects()`](03-configuration-validation-fr.md#la-validation-inter-pollers--load_foreign_objects)
  * [Valider une configuration de poller : l'endpoint gRPC `CheckPollerConfig`](03-configuration-validation-fr.md#valider-une-configuration-de-poller--lendpoint-grpc-checkpollerconfig)
  * [Calcul de la différence](03-configuration-validation-fr.md#calcul-de-la-différence)
  * [Écriture de la configuration en base de données](03-configuration-validation-fr.md#écriture-de-la-configuration-en-base-de-données)
  * [Cas épineux](03-configuration-validation-fr.md#cas-épineux)
  * [Objets transverses](03-configuration-validation-fr.md#objets-transverses)
  * [De la nécessité du cache centralisé](03-configuration-validation-fr.md#de-la-nécessité-du-cache-centralisé)
* [Streams sql/storage](04-centralized-cache-fr.md#streams-sqlstorage)
* [Cache centralisé Broker](04-centralized-cache-fr.md#cache-centralisé-broker)
  * [Alimentation du cache](04-centralized-cache-fr.md#alimentation-du-cache)
  * [Ce qui est persisté à l'arrêt](04-centralized-cache-fr.md#ce-qui-est-persisté-à-larrêt)
  * [Renseignement du `poller_id` des hosts dans le cache](04-centralized-cache-fr.md#renseignement-du-poller_id-des-hosts-dans-le-cache)
  * [Mode *legacy*](04-centralized-cache-fr.md#mode-legacy)
  * [Evolutions possibles](04-centralized-cache-fr.md#evolutions-possibles)
* [Rétention et stream RRD](05-retention-rrd-fr.md#rétention-et-stream-rrd)
  * [Problème actuel](05-retention-rrd-fr.md#problème-actuel)
  * [Architecture proposée](05-retention-rrd-fr.md#architecture-proposée)
  * [Format du buffer](05-retention-rrd-fr.md#format-du-buffer)
  * [Gestion des déconnexions multiples](05-retention-rrd-fr.md#gestion-des-déconnexions-multiples)
  * [Intégration avec le rebuild existant](05-retention-rrd-fr.md#intégration-avec-le-rebuild-existant)
  * [Cas limites](05-retention-rrd-fr.md#cas-limites)
  * [Concurrence](05-retention-rrd-fr.md#concurrence)
  * [Plan de migration](05-retention-rrd-fr.md#plan-de-migration)
* [Remote Servers et configuration centralisée](06-remote-servers-fr.md#remote-servers-et-configuration-centralisée)
  * [Situation actuelle](06-remote-servers-fr.md#situation-actuelle)
  * [Identification d'un relais](06-remote-servers-fr.md#identification-dun-relais)
  * [Nouveaux messages BBDO](06-remote-servers-fr.md#nouveaux-messages-bbdo)
  * [Topologie dans broker\_cache](06-remote-servers-fr.md#topologie-dans-broker_cache)
  * [Persistance de la topologie](06-remote-servers-fr.md#persistance-de-la-topologie)
  * [Scénario 1 : connexion d'un poller au remote](06-remote-servers-fr.md#scénario-1--connexion-dun-poller-au-remote)
  * [Scénario 1b : chaîne de relais (multi-hop)](06-remote-servers-fr.md#scénario-1b--chaîne-de-relais-multi-hop)
  * [Scénario 2 : configuration poussée par PHP](06-remote-servers-fr.md#scénario-2--configuration-poussée-par-php)
  * [Scénario 3 : remote hors-ligne puis reconnexion](06-remote-servers-fr.md#scénario-3--remote-hors-ligne-puis-reconnexion)
  * [Scénario 4 : redémarrage du central](06-remote-servers-fr.md#scénario-4--redémarrage-du-central)
  * [Migration d'un poller entre deux remotes](06-remote-servers-fr.md#migration-dun-poller-entre-deux-remotes)
  * [Endpoint gRPC GetTopology](06-remote-servers-fr.md#endpoint-grpc-gettopology)
  * [Stockage des fichiers .prot](06-remote-servers-fr.md#stockage-des-fichiers-prot)
  * [Évolution de broker\_state](06-remote-servers-fr.md#évolution-de-broker_state)
  * [Modifications nécessaires](06-remote-servers-fr.md#modifications-nécessaires)
  * [Mise en place](06-remote-servers-fr.md#mise-en-place)
* [Gestion centralisée des downtimes et acquittements](07-downtimes-acknowledgements-fr.md#gestion-centralisée-des-downtimes-et-acquittements)
  * [Problème](07-downtimes-acknowledgements-fr.md#problème)
  * [Solution : le paramètre notification_mode](07-downtimes-acknowledgements-fr.md#solution--le-paramètre-notification_mode)
  * [Persistance](07-downtimes-acknowledgements-fr.md#persistance)
  * [Migration et downtimes / acquittements](07-downtimes-acknowledgements-fr.md#migration-et-downtimes--acquittements)
  * [Pilotage de la décision de notification côté Broker](07-downtimes-acknowledgements-fr.md#pilotage-de-la-décision-de-notification-côté-broker)
* [BAM dans Broker](08-bam-fr.md#bam-dans-broker)
  * [Les objets](08-bam-fr.md#les-objets)
  * [Deux streams, un module](08-bam-fr.md#deux-streams-un-module)
  * [Le démarrage, dans les deux régimes](08-bam-fr.md#le-démarrage-dans-les-deux-régimes)
  * [Les définitions circulaires](08-bam-fr.md#les-définitions-circulaires)
  * [Ce qui a été mesuré, et où](08-bam-fr.md#ce-qui-a-été-mesuré-et-où)
* [Travaux préparatoires avant le Poller HA](09-ha-preparatory-work-fr.md#travaux-préparatoires-avant-le-poller-ha)
  * [Prérequis : les timeperiods doivent être gérables par Broker](09-ha-preparatory-work-fr.md#prérequis--les-timeperiods-doivent-être-gérables-par-broker)
  * [File `neb` à triple priorité](09-ha-preparatory-work-fr.md#file-neb-à-triple-priorité)
  * [BAM : lecture de l'état des downtimes depuis le cache Broker](09-ha-preparatory-work-fr.md#bam--lecture-de-létat-des-downtimes-depuis-le-cache-broker)
  * [Implémentation de notification_mode = broker sur zones mono-poller](09-ha-preparatory-work-fr.md#implémentation-de-notification_mode--broker-sur-zones-mono-poller)
  * [Stratégie de test](09-ha-preparatory-work-fr.md#stratégie-de-test)
  * [Prérequis 5 : élimination de `retention.dat`](09-ha-preparatory-work-fr.md#prérequis-5--élimination-de-retentiondat)
* [Poller HA](10-poller-ha-fr.md#poller-ha)
  * [Arborescence de configuration des pollers](10-poller-ha-fr.md#arborescence-de-configuration-des-pollers)
  * [Auto-surveillance d'Engine](10-poller-ha-fr.md#auto-surveillance-dengine)
  * [Architecture du protocole HA](10-poller-ha-fr.md#architecture-du-protocole-ha)
* [Tickets](11-tickets-and-issues-fr.md#tickets)
  * [Premiers tickets](11-tickets-and-issues-fr.md#premiers-tickets)
  * [Plusieurs tickets en parallèle](11-tickets-and-issues-fr.md#plusieurs-tickets-en-parallèle)
  * [Évolutions](11-tickets-and-issues-fr.md#évolutions)
* [Soucis potentiels à résoudre](11-tickets-and-issues-fr.md#soucis-potentiels-à-résoudre)
* [Résolution des soucis](11-tickets-and-issues-fr.md#résolution-des-soucis)
  * [Déplacement de l'envoi des commandes externes sur Broker](11-tickets-and-issues-fr.md#déplacement-de-lenvoi-des-commandes-externes-sur-broker)
<!-- TOC -->
