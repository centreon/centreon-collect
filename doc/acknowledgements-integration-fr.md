# Acquittements — intégration Engine ↔ Broker

<!-- TOC -->
* [Acquittements — intégration Engine ↔ Broker](#acquittements--intégration-engine--broker)
  * [Qu'est-ce qu'un acquittement ?](#quest-ce-quun-acquittement-)
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

## Qu'est-ce qu'un acquittement ?

Quand un host ou un service tombe en problème (DOWN, UNREACHABLE, WARNING, CRITICAL…), Centreon
prévient les contacts concernés et **répète** ces notifications à intervalle régulier tant que le
problème dure. Un **acquittement** est le geste par lequel un opérateur dit *« j'ai vu ce problème,
je le prends en charge »* : il **fait taire les notifications de rappel** pour cette ressource sans
rien changer à sa supervision — la ressource reste en problème, elle est simplement marquée comme
« prise en compte ». Un commentaire (auteur, explication) l'accompagne en général et reste visible
dans l'interface.

Un acquittement porte donc toujours sur **l'état d'un host/service en problème**, jamais sur une
notification particulière. On ne peut acquitter que ce qui est effectivement en problème, et
l'acquittement **disparaît de lui-même** au retour à la normale (OK/UP). Il en existe deux variantes :

- **normal** : l'acquittement saute dès que l'état change ;
- **sticky** (« collant ») : il persiste tant que la ressource n'est pas complètement guérie, même
  si elle passe d'un état de problème à un autre (p. ex. CRITICAL → WARNING).

Le reste de ce document décrit comment cet état — **décidé** par Engine — est désormais **stocké,
fermé et persisté** par Broker.

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

## Mode `notification_mode = broker` : Broker est l'autorité d'acquittement

`broker/core/inc/com/centreon/broker/broker_acknowledgement_manager.hh`, `broker/core/src/broker_acknowledgement_manager.cc`

Tout ce qui précède décrit le mode `engine` : Engine pose et lève l'acquittement, Broker le suit. En
`notification_mode = broker`, Engine **n'est jamais informé** des acquittements (voir
[Gestion centralisée des downtimes et acquittements](./nego-engine-broker-fr.md#gestion-centralisée-des-downtimes-et-acquittements)).
Broker les reçoit par `BrokerRpc`, les stocke, les exporte en base et les lève lui-même. Le
`broker_acknowledgement_manager` est un singleton chargé par `broker_state` dans ce mode seulement,
à côté du `downtime_manager` ; tout l'état vit dans `broker_cache`, la classe ne fait qu'orchestrer.

### Points d'entrée gRPC

```proto
rpc AcknowledgeHostProblem(AcknowledgementRequest) returns (google.protobuf.Empty) {}
rpc AcknowledgeServiceProblem(AcknowledgementRequest) returns (google.protobuf.Empty) {}
rpc RemoveHostAcknowledgement(HostIdentifier) returns (google.protobuf.Empty) {}
rpc RemoveServiceAcknowledgement(ServiceIdentifier) returns (google.protobuf.Empty) {}

message AcknowledgementRequest {   // mêmes champs que EngineAcknowledgement (engine.proto)
  string host_name = 1; string service_desc = 2;
  string ack_author = 3; string ack_data = 4;
  enum Type { NORMAL = 0; STICKY = 1; } Type type = 5;
  bool notify = 6; bool persistent = 7;
}
message HostIdentifier { oneof host { string host_name = 1; uint64 host_id = 2; } }
```

`AcknowledgementRequest` reprend champ pour champ la requête d'Engine : PHP change de cible gRPC et de
nom de RPC, pas de contenu. Les échecs passent par le statut gRPC : `UNAVAILABLE` hors mode broker,
`NOT_FOUND` pour une ressource inconnue du cache, `FAILED_PRECONDITION` si la ressource est UP/OK
(« cannot acknowledge a non-existent problem », comme Engine), `INVALID_ARGUMENT` si l'identifiant est
vide.

### Pose d'un acquittement : le flux d'Engine, rejoué par Broker

`broker_acknowledgement_manager::acknowledge()` reproduit `acknowledge_host_problem()` /
`acknowledge_service_problem()` d'Engine, étape par étape :

| Étape Engine | Équivalent Broker |
|---|---|
| refus si UP/OK | état lu dans le cache, `FAILED_PRECONDITION` |
| commentaire `acknowledgment` interne | `pb_comment` (entry_type ACKNOWLEDGMENT, source INTERNAL, id dans la plage partitionnée de Broker, **même `entry_time` que l'ack** : la GUI joint les deux lignes dessus) |
| `broker_acknowledgement_data` → `pb_acknowledgement` | `pb_acknowledgement` publié, avec `instance_id` du poller du host, `state` courant et le nouveau champ `comment_id` (13) |
| `set_acknowledgement(type)` + `update_status(STATUS_ACKNOWLEDGEMENT)` | `broker_cache::set_acknowledgement_type()` **synchrone** (la prochaine décision de notification doit voir le flag), puis `pb_adaptive_*_status{acknowledgement_type}` pour la base |
| `notify(reason_acknowledgement)` si `notify` | `notification_manager::notify(reason_acknowledgement, author, data)` ; l'exécution part vers le poller par `pb_notification_execute` |
| ligne `EXTERNAL COMMAND: ACKNOWLEDGE_*` → `logs` | `pb_log_entry` msg_type `SERVICE_ACKNOWLEDGE_PROBLEM` (10) / `HOST_ACKNOWLEDGE_PROBLEM` (11), auteur dans `notification_contact`, commentaire dans `output` |

Un nouvel acquittement sur une ressource déjà acquittée remplace le précédent et supprime son
commentaire non persistant, comme `delete_acknowledgement_comment()` côté Engine.

### Levée

* **Explicite** (`remove()`) : `set_acknowledgement_type(NONE)` applique la règle de fermeture de
  `_take_expired_acknowledgement` avec l'état courant — la ressource est encore en problème, donc
  l'ack reçoit son `deletion_time` et est republié ; le commentaire non persistant est supprimé ; un
  adaptive status `NONE` réaligne `hosts`/`services`/`resources`.
* **Automatique** (`clear_on_state_change()`), appelée par `broker_notification_dispatcher` pour **chaque**
  `pb_host_status`/`pb_service_status` (SOFT compris), *avant* la décision de notification, avec la
  règle de `notifier::handle_state()` : un ack NORMAL est levé à tout changement d'état, un ack STICKY
  seulement au retour UP/OK. L'ordre compte : la sortie d'un ack non sticky (WARNING → CRITICAL) doit
  être notifiée, donc le flag doit être retombé quand `notify()` lit `get_state().acknowledged`. La
  règle de fermeture est appliquée avec l'état **de l'événement**, pas celui du cache, pour rester
  déterministe : à la guérison l'ack est retiré du cache sans `deletion_time`, exactement comme en
  mode engine (voir « Fermeture »).

### Les gardes : ne pas laisser Engine écraser Broker

Engine ne connaît pas l'ack, donc chaque status qu'il envoie porte `acknowledgement_type = NONE`. Sans
garde, le cache remettrait le type à NONE et unified_sql `acknowledged = 0` à chaque check. Même
solution que pour `scheduled_downtime_depth` :

* `broker_cache::update_host/update_service(pb_*_status)` n'appliquent le type venant d'Engine que si
  `notification_manager::is_loaded()` est faux ;
* `unified_sql` binde `NULL` sur `acknowledged` / `acknowledgement_type` des requêtes de status
  (`hosts`, `services`, `resources`) et les requêtes portent `COALESCE(?, colonne)`.

Le fait « Broker est propriétaire » est lu par `notification_manager::is_loaded()`, chargé au même
endroit que le manager d'acquittement.

### Redémarrages

* **Broker redémarre** : les acks sont rechargés depuis `BrokerCache.acknowledgements` (comment_id
  compris), mais les hosts/services du cache sont reconstruits avec le type NONE, et Engine ne peut
  pas le rétablir. Deux mécanismes le restaurent : `broker_cache::_restore_acknowledgement_type()` à
  l'insertion d'une définition `pb_host`/`pb_service` (mode BBDO3 non centralisé), et
  `reinject_pending_acknowledgements()` aux deux points de réinjection des downtimes (barrière de
  démarrage, `_process_engine_state` après merge en mode centralisé). Dans les deux cas un adaptive
  status est republié, car les définitions qui ont reconstruit les ressources en base ont écrit
  `acknowledged = 0`.
* **Engine redémarre** : il renvoie ses définitions avec le type NONE ; `_restore_acknowledgement_type`
  remet le flag en place (copie-sur-écriture, l'événement entrant est partagé avec les flux de sortie).

### Ce qu'Engine perd dans ce mode (à traiter)

* Les macros `$TOTAL*UNHANDLED$` comptent un problème acquitté comme non traité (déjà vrai pour les
  downtimes) — correction prévue par un miroir descendant vers le poller, voir la note dédiée dans
  `nego-engine-broker`.
* L'expiration par `acknowledgement_timeout` est un timer Engine : elle ne se déclenche plus. Non
  portée côté Broker pour l'instant.

### Tests

`tests/broker-engine/acknowledgements-broker.robot` (`BEACKBRK1` à `BEACKBRK5`) : pose et levée à la
guérison, levée explicite, sticky/normal sur changement d'état, ack d'hôte survivant à un redémarrage
de Broker, et décision de notification (CRITICAL, ACKNOWLEDGEMENT, suppression pendant l'ack,
RECOVERY). UT : `BrokerNotificationDeliverTest.{EngineStatusKeepsBrokerAcknowledgement,
AcknowledgementClosingRule, ReinjectPendingAcknowledgements}`.

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
