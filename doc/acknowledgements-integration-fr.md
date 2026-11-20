# Acquittements — intégration Engine ↔ Broker

<!-- TOC -->
* [Acquittements — intégration Engine ↔ Broker](#acquittements--intégration-engine--broker)
  * [Vue d'ensemble](#vue-densemble)
  * [Où vit l'état d'acquittement](#où-vit-létat-dacquittement)
  * [L'état d'acquittement (côté Engine)](#létat-dacquittement-côté-engine)
  * [Création](#création)
  * [Fermeture (la seule « mise à jour »)](#fermeture-la-seule--mise-à-jour-)
    * [Guérison et changement d'état non-sticky](#guérison-et-changement-détat-non-sticky)
    * [Suppression explicite](#suppression-explicite)
  * [Côté Broker : le cache](#côté-broker--le-cache)
    * [Stocker un acquittement](#stocker-un-acquittement)
    * [Fermer un acquittement](#fermer-un-acquittement)
  * [BBDO2 vs BBDO3](#bbdo2-vs-bbdo3)
  * [Persistance](#persistance)
  * [Le commentaire d'acquittement](#le-commentaire-dacquittement)
  * [Côté Broker (unified_sql)](#côté-broker-unified_sql)
  * [gRPC : GetAcknowledgements](#grpc--getacknowledgements)
  * [Lien avec la HA des pollers](#lien-avec-la-ha-des-pollers)
  * [Avant / après](#avant--après)
<!-- TOC -->

---

## Vue d'ensemble

Le *suivi* des acquittements vivait dans **cbmod** (le module NEB linké dans `centengine`) : une map
par ressource mémorisait l'acquittement ouvert pour que, à la guérison de la ressource, cbmod puisse
émettre l'événement de fermeture. Ce suivi a été déplacé dans le **cache global** de Broker
(`broker_cache`, dans `cbd`). Engine **décide** toujours des acquittements (il porte l'état du
notifier et frappe les événements) ; Broker les **stocke, ferme et persiste** désormais.

C'est important pour la HA des pollers : l'état d'acquittement est maintenant tenu au **centre** et
**durable** au redémarrage de `cbd`, au lieu de ne vivre que dans le process poller (fongible). Voir
[Lien avec la HA des pollers](#lien-avec-la-ha-des-pollers).

> **Note.** L'*effet* de l'acquittement (suppression des notifications) reste appliqué par le notifier
> d'Engine ; seul le *bookkeeping* nécessaire à la fermeture de la ligne `acknowledgements` est passé
> côté Broker. Contrairement aux downtimes, il n'y a pas de librairie partagée
> `common/acknowledgements` — le cache suit directement les événements qui transitent par le
> multiplexer.

---

## Où vit l'état d'acquittement

Trois couches ; seules les deux premières portent de l'état vivant.

| Couche | Ce qu'elle porte | Survit à… |
|---|---|---|
| **Engine** (`centengine`, notifier) | `_acknowledgement_type`, `_acknowledgement_comment_id`, `_last_acknowledgement` — pilote la suppression de notif | un redémarrage d'Engine (via la **rétention** Engine) |
| **Broker** (`cbd`, `broker_cache`) | la map des acquittements ouverts `_acknowledgements`, clé `(host_id, service_id)` — pilote l'événement de fermeture | un redémarrage de `cbd` (via le **fichier cache** persisté, voir [Persistance](#persistance)) |
| **Base de données** | table `acknowledgements` (historique) + flags `acknowledged` / `acknowledgement_type` sur `hosts`/`services`/`resources` + le `comment` d'ack | toujours |

La source de vérité de « cette ressource est-elle acquittée » est le **notifier Engine** ; le cache
et la DB en sont des copies aval. La map du cache ne contient que les acquittements **ouverts**.

---

## L'état d'acquittement (côté Engine)

`engine/inc/com/centreon/engine/notifier.hh`, `engine/src/notifier.cc`

| Champ | Type | Signification |
|---|---|---|
| `_acknowledgement_type` | `AckType { NONE = 0, NORMAL, STICKY }` (`bbdo/neb.proto`) | Acquittement courant (NONE = non acquitté) |
| `_acknowledgement_comment_id` | `uint64_t` | Id du commentaire associé (non-persistant seulement ; voir [Le commentaire d'acquittement](#le-commentaire-dacquittement)) |
| `_last_acknowledgement` | `time_t` | Moment de la pose |

`set_acknowledgement(AckType)` (`notifier.cc:1125`) ne fait que muter le champ en mémoire ; il
n'émet aucun événement de lui-même. Ces trois champs sont écrits dans / restaurés depuis la rétention
Engine (`retention/applier/host.cc`, `retention/applier/service.cc`), donc l'acquittement survit à un
redémarrage d'**Engine**.

---

## Création

`ACKNOWLEDGE_HOST_PROBLEM` / `ACKNOWLEDGE_SVC_PROBLEM` (commande externe, `commands.cc`) ou les
handlers gRPC d'acquittement posent le notifier et émettent un événement :

```
acknowledge_*_problem  (commands.cc / gRPC)
   ├─ notifier::set_acknowledgement(NORMAL | STICKY)
   ├─ création du commentaire d'acquittement   (entry_type = acknowledgment)
   └─ broker_acknowledgement_data(...)          (engine/src/broker.cc)
        └─ forward_pb_acknowledgement  →  pb_acknowledgement   (BBDO3)
           forward_acknowledgement     →  neb::acknowledgement (BBDO2)
                                         ─▶ Broker
```

L'événement émis porte `host_id`/`service_id` (`service_id == 0` ⇒ ack d'hôte), `sticky`, `state`
(l'état au moment de l'acquittement), `author`, `comment_data`, `entry_time`, et `deletion_time`
laissé à `0` (acquittement **ouvert**).

---

## Fermeture (la seule « mise à jour »)

Un acquittement est immuable ; la seule transition est la **fermeture** (pose de `deletion_time`).
Elle n'est **pas** émise directement — elle est *dérivée du status de la ressource* par Broker. Deux
situations.

### Guérison et changement d'état non-sticky

Quand Engine débraye l'acquittement en interne (la ressource a guéri, ou un ack non-sticky a changé
d'état), le status suivant porte `acknowledgement_type == NONE`. Le cache de Broker détecte la
transition (un ack en cache + un status disant NONE) dans
`broker_cache::_take_expired_acknowledgement()` et :

* **retire** l'entrée de la map du cache, et
* publie un acquittement de **fermeture** (pose de `deletion_time`) **sauf si** la ressource est
  simplement repassée OK ou qu'un ack non-sticky a changé d'état — exactement la condition historique
  `!(!state || (!sticky && state != ack_state))`.

Ainsi une guérison vers OK vide l'entrée du cache sans écrire de `deletion_time` (un ack sticky reste
ouvert en base jusqu'à suppression explicite), ce qui préserve le comportement historique.

### Suppression explicite

`REMOVE_HOST_ACKNOWLEDGEMENT` / `REMOVE_SVC_ACKNOWLEDGEMENT` →
`remove_host_acknowledgement` / `remove_service_acknowledgement` (`engine/src/commands/commands.cc`) :

```cpp
svc->set_acknowledgement(AckType::NONE);            // débraye le notifier
svc->update_status(host::STATUS_ACKNOWLEDGEMENT);   // émet un status portant ack_type = NONE
svc->delete_acknowledgement_comment();              // supprime le commentaire (séparé)
```

Il n'y a **aucun** événement dédié de suppression d'acquittement : la fermeture de la ligne
`acknowledgements` repose sur le même chemin de détection par status que ci-dessus. Comme le service
est encore dans un état non-OK au moment de la suppression, la condition ferme la ligne
(`deletion_time` est posé).

> C'est pourquoi le routage des **status legacy** dans le cache compte en BBDO2 — voir
> [BBDO2 vs BBDO3](#bbdo2-vs-bbdo3). Avant ce routage, une suppression explicite en BBDO2 laissait la
> ligne `acknowledgements` ouverte.

---

## Côté Broker : le cache

`broker/core/cache/broker_cache.cc`

Les événements d'acquittement et les événements de status transitent tous deux par
`broker_cache::_publish()`.

### Stocker un acquittement

`update_acknowledgement()` est gated sur la section correspondant à la ressource — `CACHE_HOSTS` pour
un ack d'hôte (`service_id == 0`), `CACHE_SERVICES` pour un ack de service (les deux sont activées par
un output `unified_sql`). Il distingue ouverture/fermeture sur `deletion_time` :

```cpp
if (obj.deletion_time() > 0)
  _acknowledgements.erase({host_id, service_id});            // fermeture → on retire
else
  _acknowledgements.insert_or_assign({host_id, service_id}, ack);  // ouverture → on stocke
```

Cette garde est essentielle : l'événement de fermeture publié par le cache lui-même revient via le
multiplexer, donc une fermeture doit **retirer** l'entrée, pas la ré-insérer.

### Fermer un acquittement

Chaque handler de status (`update_host` / `update_service`, formes complètes `pb_*_status` **et**
adaptatives `pb_adaptive_*_status`) appelle `_take_expired_acknowledgement(host_id, service_id,
ack_type, state)` **sous le verrou du cache**. Cette méthode retire l'entrée et, quand l'ack doit
être fermé, pose `deletion_time` et **retourne** l'événement pour que **l'appelant le publie après
avoir relâché le verrou** — publier sous le verrou ré-entrerait dans
`_publish()`/`update_acknowledgement()` et risquerait un interblocage ré-entrant / d'ordre de verrous
avec le multiplexer.

---

## BBDO2 vs BBDO3

Le cache raisonne en protobuf. Les événements **BBDO3** natifs (`pb_acknowledgement`,
`pb_service_status`, `pb_host_status` et les formes adaptatives) sont traités directement. Les
événements **BBDO2** legacy sont convertis d'abord, dans `broker/neb/src/bbdo2_to_bbdo3.cc` :

| Événement legacy | Convertisseur | Cas `_publish` |
|---|---|---|
| `neb::acknowledgement` | `_acknowledgement_to_pb` | `de_acknowledgement` |
| `neb::service_status` | `_service_status_to_pb` | `de_service_status` |
| `neb::host_status` | `_host_status_to_pb` | `de_host_status` |

Les deux modes lancent un output `unified_sql` par défaut, donc les sections du cache sont activées
dans **les deux**. Le seul point spécifique à la version BBDO est le **type d'événement** : un poller
BBDO2 émet des `service_status` / `host_status` legacy, qui doivent être routés vers `update_service`
/ `update_host` (via `bbdo2_to_bbdo3`) pour que la détection de fermeture tourne. Sans les cas
`de_service_status` / `de_host_status`, un acquittement BBDO2 est créé et stocké mais **jamais fermé**
à la guérison ou à la suppression explicite.

---

## Persistance

`broker_cache` persiste ses acquittements ouverts pour qu'ils survivent à un redémarrage de **`cbd`**,
en miroir de la persistance des downtimes actifs :

* `BrokerCache.acknowledgements` — un champ `repeated Acknowledgement` dans `bbdo/neb.proto`.
* `_save_cache()` écrit la map `_acknowledgements` vivante (modes legacy et centralisé).
* `_load_cache()` les recharge dans la map à la construction **sans republier** — les lignes DB
  existent déjà ; on ne reconstruit que le suivi mémoire, pour que le cache puisse encore fermer un
  acquittement à une guérison ultérieure.

Cela comble un trou de l'ancien design : la map de suivi de cbmod n'était **pas** persistée et
n'était **pas** reconstruite au redémarrage (la rétention Engine restaure le champ du notifier mais
ne ré-émet pas d'événement d'acquittement), donc le bookkeeping de fermeture-à-la-guérison était perdu
au restart. L'*effet* de l'acquittement survivait toujours à un redémarrage d'**Engine** via la
rétention.

---

## Le commentaire d'acquittement

Créer un acquittement crée aussi un `comment` avec `entry_type = acknowledgment` (valeur `4`). L'id
d'un commentaire d'ack **non-persistant** est conservé sur `notifier::_acknowledgement_comment_id` et
supprimé quand l'acquittement est débrayé ; un commentaire persistant n'est pas suivi et survit. Voir
[comments-integration-fr.md](comments-integration-fr.md).

---

## Côté Broker (unified_sql)

`broker/unified_sql/src/stream_sql.cc` — `_process_pb_acknowledgement` (BBDO3) et
`_process_acknowledgement` (BBDO2) :

* **Création** (`deletion_time` absent) → `INSERT ... ON DUPLICATE KEY UPDATE` dans la table
  `acknowledgements` (clé unique `(entry_time, host_id, service_id)`).
* **Fermeture** (`deletion_time` posé) → la même ligne est mise à jour avec son `deletion_time`.

En parallèle, les flags `acknowledged` / `acknowledgement_type` des lignes `hosts` / `services` /
`resources` sont maintenus depuis les événements de status — donc le flag « acquitté » de l'UI se
nettoie à la guérison même si aucun événement de fermeture n'est écrit (p.ex. une simple guérison en
BBDO2).

---

## gRPC : GetAcknowledgements

`broker/core/brokerrpc/broker.proto`, `broker_impl.cc`

```proto
rpc GetAcknowledgements(google.protobuf.Empty) returns (AcknowledgementList) {}
message AcknowledgementList { repeated Acknowledgement entries = 1; }  // Acknowledgement de neb.proto
```

`broker_impl::GetAcknowledgements` renvoie un `Acknowledgement` par acquittement en cache (les acks
d'hôte portent `service_id == 0`), ou `UNAVAILABLE` quand ni `CACHE_HOSTS` ni `CACHE_SERVICES` n'est
activée. Il lit un snapshot via `broker_cache::acknowledgements()`. Ce point d'entrée expose le
contenu du cache pour le diagnostic et les tests (c'est sur lui que s'appuient les tests Robot
`BEACK9`/`BEACK10`).

---

## Lien avec la HA des pollers

En déplaçant le suivi des acquittements de cbmod (le process poller fongible) vers `broker_cache` (le
centre) et en le rendant durable, l'état d'acquittement peut suivre une ressource relocalisée d'un
poller à un autre : le centre le porte, au lieu qu'il soit lié à la rétention d'un seul poller. C'est
une des étapes « Broker porte un état durable » décrites dans
[Architecture cible — vers la HA des pollers](./ha-target-architecture-fr.md).

---

## Avant / après

| Aspect | Avant (cbmod) | Maintenant (broker_cache) |
|---|---|---|
| Suivi des acks ouverts | map `cbmod::_acknowledgements`, dans le process **Engine** | `broker_cache::_acknowledgements`, dans **`cbd`** |
| Fermeture à la guérison / suppression | émise par cbmod à chaque status | émise par le cache via `_take_expired_acknowledgement` |
| Fermeture BBDO2 | faite par cbmod (indépendante de l'output) | nécessite le routage des status legacy dans le cache (`de_service_status` / `de_host_status`) |
| Persistance au restart `cbd` | aucune (map perdue, non reconstruite) | persistée dans `BrokerCache.acknowledgements`, rechargée sans republier |
| Observabilité | aucune | gRPC `GetAcknowledgements` |
| Suppression de notif | notifier Engine | notifier Engine (inchangé) |
