# Commentaires — intégration Engine ↔ Broker

<!-- TOC -->
* [Commentaires — intégration Engine ↔ Broker](#commentaires--intégration-engine--broker)
  * [Vue d'ensemble](#vue-densemble)
  * [L'objet comment (côté Engine)](#lobjet-comment-côté-engine)
  * [Qui mémorise un id de commentaire](#qui-mémorise-un-id-de-commentaire)
  * [Création](#création)
  * [Suppression](#suppression)
    * [Suppression par id](#suppression-par-id)
    * [Suppression en masse](#suppression-en-masse)
  * [Côté Broker (unified_sql)](#côté-broker-unified_sql)
  * [Expiration (supprimée)](#expiration-supprimée)
  * [Rétention](#rétention)
  * [status.dat](#statusdat)
  * [Cycle de vie par type](#cycle-de-vie-par-type)
    * [user](#user)
    * [downtime](#downtime)
    * [flapping](#flapping)
    * [acquittement](#acquittement)
  * [Identité : comment_id vs internal_id](#identité--comment_id-vs-internal_id)
  * [Avant / après](#avant--après)
<!-- TOC -->

---

## Vue d'ensemble

La gestion des commentaires vivait **entièrement dans Engine**, dans une map mémoire statique
(`comment::comments`). Elle a été retravaillée pour qu'**Engine ne conserve aucun commentaire en
mémoire** : Broker est désormais **propriétaire** des commentaires (table
`centreon_storage.comments`). Engine se contente de :

* émettre un événement de création quand un commentaire est créé ;
* émettre un événement de suppression (par id, ou en masse par cible) quand un commentaire doit
  disparaître ;
* mémoriser, sur l'objet runtime porteur, les quelques ids de commentaires nécessaires pour piloter
  ces suppressions.

> **Note.** Contrairement aux downtimes, les commentaires n'ont **pas** eu de librairie partagée
> `common/comments` avec un singleton `comment_manager` et des callbacks injectées. Une approche plus
> légère a été retenue : l'objet `comment` reste dans Engine mais devient **éphémère** (il n'existe
> que le temps d'émettre son événement de création), et Broker possède le stockage. Il n'y a donc
> aucune abstraction `comment_manager` / `comment_callbacks` à implémenter.

Un commentaire a l'un de quatre **types d'entrée** (`entry_type`) : `user`, `downtime`, `flapping`,
`acknowledgment`.

---

## L'objet comment (côté Engine)

`engine/inc/com/centreon/engine/comment.hh`, `engine/src/comment.cc`

| Champ | Type | Signification |
|---|---|---|
| `_comment_type` | `enum type { host = 1, service }` | Objet porteur |
| `_entry_type` | `enum e_type { user = 1, downtime, flapping, acknowledgment }` | Origine du commentaire |
| `_comment_id` | `uint64_t` | Identifiant minté par Engine (stocké en base comme `internal_id`) |
| `_source` | `enum src { internal, external }` | Créé par le moteur ou par une commande externe |
| `_persistent` | `bool` | Survit aux redémarrages / non auto-supprimé |
| `_entry_time` | `time_t` | Date de création |
| `_expires` / `_expire_time` | `bool` / `time_t` | Présents mais **toujours `false`/`0`** — l'expiration est inutilisée (cf. [Expiration](#expiration-supprimée)) |
| `_host_id` / `_service_id` | `uint64_t` | Objet cible (`_service_id == 0` ⇒ commentaire d'hôte) |
| `_author` | `std::string` | Auteur |
| `_comment_data` | `std::string` | Texte |

L'objet est **éphémère** : il est construit sur la pile au site de création, son constructeur minte
un id et émet l'événement de création, puis il est détruit. Il n'y a **plus de map mémoire**.

L'id est minté de façon monotone : `_comment_id = _next_comment_id++`. `_next_comment_id` est
persisté dans le fichier de rétention (`retention/program.cc`, clé `next_comment_id`) afin que les ids
croissent au fil des redémarrages, et il est remis à `1` au rechargement de configuration
(`applier/state.cc`).

---

## Qui mémorise un id de commentaire

Engine doit pouvoir supprimer un commentaire plus tard. Plutôt que de garder chaque commentaire en
mémoire, l'id est stocké sur l'**objet runtime porteur** — sauf pour les commentaires `user`, dont
l'id est fourni par l'IHM au moment de la suppression.

| `entry_type` | Créé par | Où l'id est gardé | Supprimé quand |
|---|---|---|---|
| `user` | `cmd_add_comment` — `ADD_HOST_COMMENT` / `ADD_SVC_COMMENT` (et le gRPC `AddHostComment`/`AddServiceComment`) | — (l'IHM fournit l'id) | `DEL_*_COMMENT` par id, `DEL_ALL_*`, ou suppression de l'objet |
| `downtime` | `engine_downtime_callbacks` quand un downtime souscrit | `downtime::_comment_id` | destruction de l'objet downtime |
| `flapping` | `host::set_flap()` / `service::set_flap()` | `notifier::_flapping_comment_id` | `clear_flap()` (fin du flapping) |
| `acknowledgment` | `cmd_acknowledge_*_problem` (et les handlers d'ack gRPC) | `notifier::_acknowledgement_comment_id` (**non-persistant uniquement**) | levée d'ack → suppression par id stocké |

Un commentaire d'acquittement persistant n'est **volontairement pas** tracké (son id reste `0`) afin
qu'il survive à la levée de l'acquittement — même sémantique qu'avant, sans scan de map.

---

## Création

```
site de création (commande / gRPC / flapping / ack / downtime)
   └─ construit un `comment(...)` éphémère
        ├─ minte internal_id (_next_comment_id++)
        └─ broker_comment_data(NEBTYPE_COMMENT_ADD, ...)  ─▶ pb_comment ─▶ INSERT Broker
   └─ (si besoin) stocke l'id sur l'objet porteur, puis détruit l'objet comment
```

Le constructeur n'émet l'événement de création **que** lorsqu'il minte l'id (aucun id fourni). Un
commentaire construit avec un id explicite (chemin de compat ascendante de la rétention) n'émet rien.

---

## Suppression

Engine ne reconstruit plus le tuple complet d'un commentaire pour le supprimer : il adresse la ligne
directement.

### Suppression par id

Utilisée par `DEL_HOST_COMMENT` / `DEL_SVC_COMMENT` et par les suppressions ciblées (destruction de
downtime, `clear_flap()`, levée d'acquittement).

`comment::delete_comment(id)` émet un `pb_comment` ne portant que `internal_id` (= l'id) et
`deletion_time` ; `host_id`/`service_id` restent à `0`. Broker retrouve la ligne sur
**`(internal_id, instance_id)`** (l'id du poller est rempli par cbmod).

### Suppression en masse

Utilisée par `DEL_ALL_HOST_COMMENTS` / `DEL_ALL_SVC_COMMENTS` et par la suppression d'objet (un
host/service retiré de la configuration).

`comment::delete_host_comments(host_id)` / `delete_service_comments(host_id, service_id)` émettent un
**seul** événement avec `internal_id = 0` (la sentinelle bulk) plus la cible `host_id`
(`+ service_id`). Broker supprime toutes les lignes correspondantes en une seule requête. Aucune
itération de map.

---

## Côté Broker (unified_sql)

`broker/unified_sql/src/stream_sql.cc`, dans **les deux** handlers de commentaire —
`_process_pb_comment` (BBDO3 `pb_comment`) et `_process_comment` (BBDO2 `neb::comment`) :

* **Création** (`deletion_time` non positionné) → l'`INSERT INTO comments ... ON DUPLICATE KEY
  UPDATE` existant (par lots via le bulk `_comments`).
* **Suppression** (`deletion_time` positionné) → un `UPDATE` dédié, car l'événement partiel ne peut
  plus matcher la clé unique :
  * `internal_id != 0` → `UPDATE comments SET deletion_time=? WHERE internal_id=? AND instance_id=?`
  * `internal_id == 0` et `service_id != 0` → `... WHERE host_id=? AND service_id=? AND instance_id=?`
  * `internal_id == 0` et `service_id == 0` → `... WHERE host_id=? AND (service_id=0 OR service_id IS NULL) AND instance_id=?` (commentaires d'hôte uniquement)

**Ordre.** Avant d'exécuter l'`UPDATE` de suppression, le handler flushe toute *création* de
commentaire encore en attente dans le bulk `_comments`, sur la **même** connexion
(`special_conn::comment`), afin que l'`UPDATE` s'applique après l'`INSERT` (FIFO préservé). C'est
important quand un commentaire est créé puis supprimé dans la même fenêtre de flush.

`internal_id == 0` est une sentinelle sûre : Engine minte les ids à partir de `1`, donc `0` ne
correspond jamais à un vrai commentaire.

---

## Expiration (supprimée)

Le mécanisme d'expiration des commentaires était du **code mort** et a été supprimé. Tout commentaire
est créé avec `expires = false` (les commandes externes ne portent aucun paramètre d'expiration ; les
commentaires de flapping, downtime et ack passent tous `false`), donc rien ne produisait jamais de
commentaire expirant. L'événement temporisé `EVENT_EXPIRE_COMMENT`, son handler et
`comment::remove_if_expired_comment()` ont disparu. Les colonnes `expires` / `expire_time` restent
dans le proto/la base pour compatibilité mais sont toujours non positionnées.

---

## Rétention

Engine n'écrit plus la **liste des commentaires** dans son fichier de rétention — Broker possède les
commentaires, et ceux-ci survivent à un redémarrage d'Engine dans la base (Broker n'est pas
redémarré avec Engine). Ce qu'Engine persiste encore est seulement ce dont il a besoin pour continuer
à piloter les suppressions :

* `flapping_comment_id` et `acknowledgement_comment_id` sur chaque bloc de rétention **host/service**
  (`retention/host.cc`, `retention/service.cc`, leurs appliers et `retention/dump.cc`) ;
* le compteur `next_comment_id` (inchangé).

Pour une montée de version en douceur, un **ancien** fichier de rétention contenant encore des blocs
`comment { ... }` est toujours parsé : `retention/applier/comment.cc` n'utilise ces blocs que pour
(1) rebrancher sur son notifier l'id d'un commentaire d'acquittement non-persistant conservé et
(2) purger les commentaires non-persistants au démarrage. Il ne crée plus aucun objet commentaire.

**Changement de comportement (assumé) :** les commentaires *user* non-persistants ne sont plus purgés
au redémarrage (aucun objet porteur ne les retient) ; c'est la purge temporelle en base
(`len_storage_comments`) qui assure leur nettoyage à long terme.

---

## status.dat

Les commentaires **ne sont plus écrits** dans `status.dat` (`xsddefault.cc`). Ce fichier legacy n'est
pas utilisé par Centreon pour lire les commentaires (l'IHM lit la base) ; seuls d'éventuels
consommateurs externes de `status.dat` sont concernés.

---

## Cycle de vie par type

### user

`ADD_HOST_COMMENT` / `ADD_SVC_COMMENT` (ou les équivalents gRPC) créent le commentaire. La suppression
est pilotée par l'IHM, qui envoie l'`internal_id` dans `DEL_*_COMMENT` (cf.
[Identité](#identité--comment_id-vs-internal_id)) ; Engine le relaie à Broker en delete-by-id.
`DEL_ALL_*` et la suppression d'un host/service déclenchent une suppression en masse.

### downtime

Cf. [downtimes-integration-fr.md](downtimes-integration-fr.md). Le downtime crée son commentaire à la
souscription et stocke `_comment_id` ; il supprime cet id à sa destruction. Le commentaire vit donc
exactement aussi longtemps que l'objet downtime. La transition `START`/`STOP` ne **touche pas** au
commentaire.

### flapping

`host::set_flap()` / `service::set_flap()` créent un commentaire `flapping` et stockent son id dans
`notifier::_flapping_comment_id` ; `clear_flap()` le supprime par id. Le commentaire vit le temps de
l'épisode de flapping. L'id est persisté en rétention, donc il survit à un redémarrage d'Engine.

### acquittement

`cmd_acknowledge_*_problem` (et les handlers d'ack gRPC) créent le commentaire. L'id d'un commentaire
d'ack **non-persistant** est stocké dans `notifier::_acknowledgement_comment_id` ; à la levée de
l'acquittement, `notifier::delete_acknowledgement_comment()` le supprime par id et remet le champ à
zéro. Un commentaire d'ack persistant n'est pas tracké et survit à la levée. L'id est persisté en
rétention ; un ancien fichier de rétention le rebranche via `retention/applier/comment.cc`.

---

## Identité : comment_id vs internal_id

La table `centreon_storage.comments` porte **deux** identifiants :

```sql
comment_id  int NOT NULL AUTO_INCREMENT,        -- PRIMARY KEY, générée par la base
internal_id int NOT NULL,                        -- = id de commentaire minté par Engine
UNIQUE KEY (entry_time, host_id, service_id, instance_id, internal_id),
KEY (internal_id)
```

* `comment_id` (PK) est entièrement géré par MariaDB ; il n'est **pas** utilisé pour adresser un
  commentaire aujourd'hui — l'IHM ne le renvoie jamais.
* `internal_id` est l'id minté par Engine, et il est **porteur** :
  1. **Handle de suppression IHM.** Lorsqu'un opérateur supprime un commentaire, l'IHM/API Centreon
     envoie une commande externe `DEL_HOST_COMMENT` / `DEL_SVC_COMMENT` portant l'**`internal_id`** —
     et *non* la PK `comment_id` (`www/include/monitoring/comments/common-Func.php`). Engine relaie cet
     id ; Broker supprime la ligne dont l'`internal_id` correspond.
  2. **Clé d'idempotence.** Il désambiguïse la clé unique (deux commentaires créés la même seconde sur
     le même host/service depuis le même poller entreraient sinon en collision) et, comme l'`INSERT
     ... ON DUPLICATE KEY UPDATE` de création n'écrit jamais `comment_id`, c'est lui qui fait qu'un
     `pb_comment` rejoué par BBDO fait un upsert sur place au lieu de dupliquer.

**Évolution future possible (non faite) :** confier l'identité à la PK `comment_id`. C'est une
**migration cross-repo** coordonnée (Centreon web PHP + Broker), précisément parce que l'IHM adresse
les commentaires par `internal_id` aujourd'hui : l'IHM enverrait `comment_id`, Broker supprimerait par
`comment_id`, et Engine cesserait de minter les ids. Même alors, `internal_id` (ou une clé stable
équivalente) reste nécessaire pour l'idempotence tant qu'Engine est le *producteur* et que BBDO peut
rejouer — il ne disparaît que si l'**origination** des commentaires migre elle-même dans Broker.

---

## Avant / après

| Aspect | Engine avant | Engine maintenant |
|---|---|---|
| Stockage | map statique `comment::comments` | aucun — Broker possède la table `comments` |
| Objet `comment` | gardé dans la map | éphémère (émet son événement de création, puis détruit) |
| Minting d'id | `next_comment_id` + scan de map | `next_comment_id++` monotone |
| Suppression par id | reconstruit le tuple complet depuis la map | émet seulement `internal_id` ; Broker `UPDATE ... WHERE internal_id AND instance_id` |
| Suppression en masse | itération de map, un événement par commentaire | un événement (`internal_id = 0`) ; Broker `UPDATE ... WHERE host_id [AND service_id]` |
| Suppression d'acquittement | scan de la map par `entry_type == ack` | suppression par id stocké sur le notifier |
| Expiration | `EVENT_EXPIRE_COMMENT` (jamais déclenché) | supprimée (code mort) |
| Rétention | liste complète des commentaires + `next_comment_id` | ids sur les objets host/service + `next_comment_id` |
| status.dat | commentaires écrits | commentaires non écrits |
