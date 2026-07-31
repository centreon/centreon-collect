# Moteur — Notifications

Ce document décrit le fonctionnement des notifications côté moteur
(`engine/`), de leur déclenchement jusqu'à l'exécution des commandes de
notification des contacts. Le cœur de la logique est la classe abstraite
`notifier` (`engine/src/notifier.cc`), dont héritent `host` et `service`.

## Sommaire

  - [1. Concepts](#1-concepts)
  - [2. Le pipeline `notify()`](#2-le-pipeline-notify)
  - [3. Choix des contacts (escalations, groupes)](#3-choix-des-contacts-escalations-groupes)
  - [4. Exécution des commandes de notification](#4-exécution-des-commandes-de-notification)
  - [5. Notifications par type](#5-notifications-par-type)
    - [5.1 PROBLEM (normal)](#51-problem-normal)
    - [5.2 RECOVERY](#52-recovery)
    - [5.3 ACKNOWLEDGEMENT](#53-acknowledgement)
    - [5.4 FLAPPING](#54-flapping)
    - [5.5 DOWNTIME](#55-downtime)
    - [5.6 CUSTOM](#56-custom)
  - [6. Paramètres de configuration](#6-paramètres-de-configuration)
  - [8. Introduction à la nouvelle notification](#8-introduction-à-la-nouvelle-notification)

## 1. Concepts

Une notification est caractérisée par un **type** (`notifier::reason_type`) qui
est regroupé en **catégorie** (`notifier::notification_category`) :

| `reason_type`                 | Catégorie (`notification_category`) | Macro `$NOTIFICATIONTYPE$` |
|-------------------------------|-------------------------------------|----------------------------|
| `reason_normal`               | `cat_normal`                        | `PROBLEM`                  |
| `reason_recovery`             | `cat_recovery`                      | `RECOVERY`                 |
| `reason_acknowledgement`      | `cat_acknowledgement`               | `ACKNOWLEDGEMENT`          |
| `reason_flappingstart`        | `cat_flapping`                      | `FLAPPINGSTART`            |
| `reason_flappingstop`         | `cat_flapping`                      | `FLAPPINGSTOP`             |
| `reason_flappingdisabled`     | `cat_flapping`                      | `FLAPPINGDISABLED`         |
| `reason_downtimestart`        | `cat_downtime`                      | `DOWNTIMESTART`            |
| `reason_downtimeend`          | `cat_downtime`                      | `DOWNTIMEEND`              |
| `reason_downtimecancelled`    | `cat_downtime`                      | `DOWNTIMECANCELLED`        |
| `reason_custom` (= 99)        | `cat_custom`                        | `CUSTOM`                   |

La correspondance type → catégorie est faite par `notifier::get_category()`.

Chaque _notifier_ mémorise la **dernière notification de chaque catégorie** dans
le tableau `_notification[cat]` (un `std::unique_ptr<notification>` par
catégorie). Ce tableau forme une petite machine à états :

- `_notification_number` : numéro de la notification de problème en cours
  (incrémenté à chaque notification sauf `recovery`, remis à zéro après recovery /
  flapping-stop, conservé pour acknowledgement et downtime).
- `_notification[cat_normal]` présent ⇒ un PROBLEM a déjà été envoyé (condition
  nécessaire pour qu'un RECOVERY parte).
- `_last_notification` / `_next_notification` : horodatages utilisés pour la
  re-notification périodique (`notification_interval`).

## 2. Le pipeline `notify()` actuel

Tout passe par `notifier::notify(type, author, data, options)`. Les états
`host`/`service` et les callbacks (downtime, commandes externes) ne font que
l'appeler avec le bon `reason_type`.

Ce système n'est pas commode car très imbriqué dans le fonctionnement d'Engine.
Le but est donc de simplifier la classe `notifier` et d'introduire un
`notification_manager` qui sera le centre de la librairie de notification
d'Engine dans un premier temps puis aussi de Broker si besoin.

Ci-dessous sont décrits les fonctionnements actuels de la notification :

```mermaid
sequenceDiagram
    participant SRC as host/service ou callback
    participant N as notifier::notify()
    participant V as is_notification_viable[cat]
    participant C as get_contacts_to_notify()
    participant E as notification::execute()

    SRC->>N: notify(type, author, data, options)
    N->>N: cat = get_category(type)
    N->>V: is_notification_viable(cat, type, options)
    alt non viable
        V-->>N: false
        N-->>SRC: OK (rien n'est envoyé)
    else viable
        V-->>N: true
        N->>N: ++_notification_number (sauf recovery)
        N->>C: get_contacts_to_notify(cat, type)
        C-->>N: ensemble de contacts + notification_interval + escalated
        N->>N: crée l'objet notification (id, number, interval, escalated)
        N->>E: execute(contacts)
        E-->>N: OK / ERROR
        alt OK
            N->>N: _last_notification = now (si contacts non vide)
            N->>N: met à jour _notification[cat] (machine à états)
            N->>N: remet _notification_number à 0 (sauf ack/downtime)
        end
        N-->>SRC: retval
    end
```

Points clés :

- Si la notification **n'est pas viable**, `notify()` retourne `OK` sans rien
  envoyer (ce n'est pas une erreur).
- Le compteur `_notification_number` est incrémenté **avant** l'envoi pour tous
  les types sauf `recovery`.
- La mise à jour de `_notification[cat]` dépend de la catégorie : un RECOVERY
  efface `cat_normal` et `cat_recovery` ; un FLAPPINGSTOP/DISABLED efface
  `cat_flapping` ; un DOWNTIMEEND/CANCELLED efface `cat_downtime`. Pour
  acknowledgement et downtime, `_notification_number` est **conservé** (sinon le
  recovery suivant ne partirait pas).

## 3. Choix des contacts (escalations, groupes)

`notifier::get_contacts_to_notify()` construit l'ensemble des contacts :

1. **Escalations d'abord** : la viabilité de chaque escalation (état couvert par
   `escalate_on`, plage `[first, last]` du numéro de notification, période
   active) et l'agrégation (mode **escaladé** dès qu'une escalation est viable,
   **plus petit** intervalle parmi les viables, union de leurs contactgroups)
   sont désormais calculées par la fonction partagée
   `notifications::evaluate_escalations()` (voir
   [Escalations : évaluation mutualisée](#escalations--évaluation-mutualisée)).
   Les contactgroups viables retournés sont ensuite résolus en contacts ici.
2. **Sinon** (aucune escalation viable) : on prend les contacts directs du
   notifier **et** les contacts de ses contactgroups.

Dans tous les cas, chaque contact n'est retenu que si
`contact::should_be_notified(cat, type, *this)` est vrai (filtre par contact :
options de notification du contact, timeperiod du contact, type accepté, etc.).
L'ensemble retourné est un `std::unordered_set` (un contact présent dans
plusieurs groupes n'est notifié qu'une fois).

## 4. Exécution des commandes de notification

`notification::execute(contacts)` (`engine/src/notification.cc`) :

```mermaid
sequenceDiagram
    participant N as notification::execute()
    participant M as nagios_macros
    participant CT as pour chaque contact
    participant NC as host/service::notify_contact()
    participant SH as my_system_r (shell)

    N->>M: grab_macros_r(parent)  (macros host/service)
    N->>M: set $NOTIFICATIONTYPE$, author/comment, number, id, escalated
    loop pour chaque contact à notifier
        N->>M: grab_contact_macros_r(contact) + clear_summary_macros_r
        N->>NC: notify_contact(mac, contact, type, author, data, options)
        loop pour chaque commande de notif du contact
            NC->>NC: get_raw_command_line_r + process_macros_r
            NC->>NC: log "HOST/SERVICE NOTIFICATION: ..." (si log_notifications)
            alt commande autorisée par la whitelist
                NC->>SH: my_system_r(commande, notification_timeout)
            else refusée
                NC->>NC: error "not allowed by the whitelist"
            end
        end
        NC-->>N: OK ⇒ contact compté, ajouté à $NOTIFICATIONRECIPIENTS$
    end
```

- Les commandes exécutées sont celles du **contact** :
  `get_host_notification_commands()` ou `get_service_notification_commands()`.
- Chaque commande est filtrée par la **whitelist** (`command_is_allowed_by_whitelist(..., NOTIF_TYPE)`)
  avant exécution par `my_system_r`, avec le timeout `notification_timeout`.
- Si `log_notifications` est activé, une ligne `HOST NOTIFICATION:` /
  `SERVICE NOTIFICATION:` est journalisée par contact et par commande.
- `notify_contact` met à jour `last_host_notification` /
  `last_service_notification` du contact.

## 5. Notifications par type

Chaque catégorie a sa propre fonction de viabilité, sélectionnée via le tableau
`_is_notification_viable[cat]`. Le drapeau `notification_option_forced` (par
exemple une notification forcée par commande externe) **court-circuite** la
plupart des vérifications.

### 5.1 PROBLEM (normal)

Déclenché quand un host/service passe en état **hard** non-OK
(`host::handle_*`, `service::handle_*` → `notify(reason_normal, …)`).

Viabilité (`_is_notification_viable_normal`) — toutes ces conditions doivent
être vraies (sauf si forcée) :

- notifications globales activées (`enable_notifications`) et activées pour le
  notifier (`get_notifications_enabled()`) ;
- notifier **pas** en downtime ; **dans** la timeperiod de notification ;
  **pas** en flapping ;
- état **hard** (un service `volatile` est notifié même hors hard) ;
- problème **non acquitté** ; état courant **différent d'OK/UP** ;
- `notify_on_current_state()` (le notifier est configuré pour notifier cet état ;
  pour un service, son host ne doit pas être down) ;
- délai de première notification écoulé (`first_notification_delay`) ;
- autorisé par les dépendances (`authorized_by_dependencies(notification)`) ;
- si un PROBLEM a déjà été envoyé : respect de l'intervalle de ré-notification
  (`notification_interval` ; `0` ⇒ pas de ré-notification).

```mermaid
sequenceDiagram
    participant CK as service::handle_async_check_result (hard, non-OK)
    participant N as notifier::notify(reason_normal)
    participant V as _is_notification_viable_normal
    participant E as execute(contacts)

    CK->>N: notify(reason_normal)
    N->>V: viable ? (enabled, hard, !downtime, !flapping,<br/>!ack, notify_on_state, delay, interval)
    alt non viable
        V-->>N: false → OK
    else viable
        V-->>N: true
        N->>N: ++_notification_number
        N->>E: execute(contacts directs/escaladés)
        E-->>N: OK → _notification[cat_normal] conservé,<br/>_notification_number remis à 0
    end
```

### 5.2 RECOVERY

Déclenché quand le notifier revient en **OK/UP** (`notify(reason_recovery, …)`).

Viabilité (`_is_notification_viable_recovery`) :

- notifications activées (globalement et pour le notifier) ;
- dans la timeperiod **sauf si** `send_recovery_notifications_anyways` est activé ;
- pas en downtime ; pas en flapping ; état **hard** ; état **OK/UP** ;
- configuré pour notifier le recovery (`notify_on(up)` / `notify_on(ok)`) ;
- délai `recovery_notification_delay` écoulé ;
- `_notification_number > 0` **et** `_notification[cat_normal]` présent : un
  PROBLEM doit avoir été envoyé auparavant.

La distinction `send_later` détermine si l'on réessaiera plus tard (hors
timeperiod, downtime, flapping, soft…) ou si l'on abandonne en réinitialisant
`_notification_number` (non configuré pour le recovery). Un recovery efface
`cat_normal` et `cat_recovery` et remet `_notification_number` à 0.

```mermaid
sequenceDiagram
    participant CK as host/service revient OK/UP
    participant N as notifier::notify(reason_recovery)
    participant V as _is_notification_viable_recovery
    participant E as execute(contacts)

    CK->>N: notify(reason_recovery)
    N->>V: viable ? (un PROBLEM envoyé ? state OK/UP ?<br/>notify_on(ok/up) ? delay ? timeperiod/anyways ?)
    alt non viable
        V-->>N: false (ré-essai plus tard ou abandon)
    else viable
        V-->>N: true (pas d'incrément du number)
        N->>E: execute(contacts du dernier PROBLEM)
        E-->>N: OK → reset cat_normal + cat_recovery,<br/>_notification_number = 0
    end
```

> Note : chaque ré-notification PROBLEM **accumule** les contacts de la
> notification normale précédente (`notif->add_contacts(...)` dans `notify()`),
> de sorte que `_notification[cat_normal]` conserve l'ensemble cumulé des
> personnes déjà prévenues. La viabilité du RECOVERY exige justement que cette
> notification normale mémorisée existe.

### 5.3 ACKNOWLEDGEMENT

Déclenché par la commande externe d'acquittement
(`commands.cc` `acknowledge_host`/`acknowledge_service` →
`notify(reason_acknowledgement, author, comment, …)`).

Viabilité (`_is_notification_viable_acknowledgement`) — minimale :

- forcée ⇒ envoyée ;
- notifications activées (globalement et pour le notifier) ;
- l'objet doit être **en problème** (état ≠ OK/UP).

`_notification_number` est **conservé** (l'acquittement ne doit pas empêcher le
recovery ultérieur).

```mermaid
sequenceDiagram
    participant EXT as commande externe ACKNOWLEDGE_*
    participant CMD as commands::acknowledge_*
    participant N as notifier::notify(reason_acknowledgement)
    participant E as execute(contacts)

    EXT->>CMD: ACKNOWLEDGE_HOST_PROBLEM / _SVC_PROBLEM
    CMD->>N: notify(reason_acknowledgement, author, comment)
    N->>N: viable ? (enabled, state != OK/UP)
    alt viable
        N->>E: execute(contacts)  (author/comment dans les macros)
        E-->>N: OK → _notification_number CONSERVÉ
    end
```

### 5.4 FLAPPING

Déclenché par la détection de flapping (`host::set_flap`/`clear_flap`,
`service::set_flap`/`clear_flap`) :
`reason_flappingstart`, `reason_flappingstop`, et `reason_flappingdisabled`
(quand la détection de flapping est désactivée alors que l'objet flappe).

Viabilité (`_is_notification_viable_flapping`) :

- forcée ⇒ envoyée ; notifications activées ;
- configuré pour ce type (`notify_on(flappingstart/stop/disabled)`) ;
- **pas de START** si une notification flapping est déjà en cours ;
- **STOP/DISABLED** uniquement si la dernière notification flapping était un
  START ;
- pas deux fois la même (même `reason`) ;
- pas pendant un downtime planifié.

```mermaid
sequenceDiagram
    participant FD as détection flapping (set_flap/clear_flap)
    participant N as notifier::notify(reason_flapping*)
    participant V as _is_notification_viable_flapping
    participant E as execute(contacts)

    FD->>N: notify(reason_flappingstart)
    N->>V: viable ? (notify_on(flappingstart), pas déjà en cours, pas downtime)
    V-->>N: true
    N->>E: execute → _notification[cat_flapping] = START
    Note over FD,N: plus tard, fin du flapping
    FD->>N: notify(reason_flappingstop)
    N->>V: viable ? (la dernière flapping était un START)
    V-->>N: true
    N->>E: execute → reset cat_flapping
```

### 5.5 DOWNTIME

Déclenché par les callbacks de downtime
(`engine/src/engine_downtime_callbacks.cc`) :
`reason_downtimestart` (début), `reason_downtimeend` (fin normale),
`reason_downtimecancelled` (annulation).

Viabilité (`_is_notification_viable_downtime`) :

- forcée ⇒ envoyée ; notifications activées (globalement et pour le notifier) ;
- configuré pour le downtime (`notify_on(downtime)`) ;
- **pas** déjà dans un downtime planifié (`get_scheduled_downtime_depth() == 0`).

Comme l'acquittement, le downtime **conserve** `_notification_number`. Un
DOWNTIMEEND/CANCELLED efface `_notification[cat_downtime]`.

```mermaid
sequenceDiagram
    participant DT as engine_downtime_callbacks
    participant N as notifier::notify(reason_downtime*)
    participant V as _is_notification_viable_downtime
    participant E as execute(contacts)

    DT->>N: notify(reason_downtimestart, author, comment)
    N->>V: viable ? (notify_on(downtime), depth == 0)
    V-->>N: true
    N->>E: execute → _notification[cat_downtime] = START,<br/>_notification_number CONSERVÉ
    Note over DT,N: à la fin / annulation
    DT->>N: notify(reason_downtimeend | reason_downtimecancelled)
    N->>E: execute → reset cat_downtime
```

### 5.6 CUSTOM

Déclenché par la commande externe de notification personnalisée
(`engine/src/commands/processing.cc` → `notify(reason_custom, author, data, …)`).

Viabilité (`_is_notification_viable_custom`) :

- forcée ⇒ envoyée ; notifications activées (globalement et pour le notifier) ;
- pas pendant un downtime planifié.

Une notification CUSTOM ne touche pas la machine à états des problèmes : c'est un
envoi ponctuel vers les contacts, avec l'auteur et le message fournis.

```mermaid
sequenceDiagram
    participant EXT as commande externe SEND_CUSTOM_*_NOTIFICATION
    participant PR as commands::processing
    participant N as notifier::notify(reason_custom)
    participant E as execute(contacts)

    EXT->>PR: SEND_CUSTOM_HOST/SVC_NOTIFICATION
    PR->>N: notify(reason_custom, author, message, options)
    N->>N: viable ? (enabled, pas en downtime)  — forçable
    N->>E: execute(contacts)  ($NOTIFICATIONTYPE$ = CUSTOM)
```

## 6. Paramètres de configuration

Paramètres influençant la viabilité et le rythme des notifications :

- **Globaux** (`pb_indexed_config.state()`) : `enable_notifications`,
  `interval_length`, `notification_timeout`, `log_notifications`,
  `send_recovery_notifications_anyways`.
- **Par notifier** : `notifications_enabled`, `notification_period`,
  `notification_interval`, `first_notification_delay`,
  `recovery_notification_delay`, `notify_on` (masque
  `up/down/unreachable/ok/warning/critical/unknown/flapping*/downtime`),
  `is_volatile`.
- **Par contact** : options de notification (host/service), timeperiod du
  contact, commandes de notification host/service — tout cela évalué dans
  `contact::should_be_notified()` puis `notify_contact()`.
- **Escalations** : intervalle propre, plage `first_notification`/
  `last_notification`, `escalate_on`, contactgroups — leur viabilité et leur
  agrégation sont évaluées par la fonction partagée
  `notifications::evaluate_escalations()` (côté Engine comme côté Broker).

## 7. Pour aller plus loin (code)

- `engine/src/notifier.cc` — `notify()`, `get_category()`,
  `get_contacts_to_notify()`, `is_notification_viable()` et les six
  `_is_notification_viable_*`.
- `engine/src/notification.cc` — `notification::execute()` (macros, itération
  contacts).
- `engine/src/host.cc` / `engine/src/service.cc` — `notify_contact()`
  (exécution des commandes, whitelist, logs) et les déclencheurs PROBLEM /
  RECOVERY / FLAPPING.
- `engine/src/engine_downtime_callbacks.cc` — déclencheurs DOWNTIME.
- `engine/src/commands/commands.cc` — déclencheur ACKNOWLEDGEMENT.
- `engine/src/commands/processing.cc` — déclencheur CUSTOM.

## 8. Introduction à la nouvelle notification

Les schémas des sections précédentes décrivent la notification **historique**,
fortement imbriquée dans `notifier`. Cette section décrit la **librairie de
notification partagée**, dont l'objectif est de rendre la notification
indépendante d'Engine. Elle vit dans **`common/notifications/`** et est
**déjà consommée par Broker** en `notification_mode = broker` (décision de
notification côté Broker, cf. `doc/nego-engine-broker-*.md`) autant que par
Engine : viabilité (`notification_manager`), filtrage des contacts
(`should_notify_contact`) et, désormais, évaluation des escalades
(`evaluate_escalations`).

### Principe

Le modèle suit celui de la librairie de downtimes (`common/downtimes`) : la
librairie ne connaît plus les objets d'Engine (`notifier`, `contact`,
`nagios_macros`, globaux…). Elle dialogue avec l'application hôte au travers
d'une **interface injectée**, `notification_callbacks`, et adresse les
ressources par **identité logique `(host_id, service_id)`** (avec
`service_id == 0` pour un host). Tout le couplage à Engine est concentré dans
**une seule** implémentation, `engine_notification_callbacks`, vivant côté
`cce_core`.

### Composants

| Composant | Emplacement | Rôle |
|---|---|---|
| `notification_manager` | lib (`common/notifications/`) | Singleton. Politique de viabilité, état runtime par `(host_id, service_id)`, orchestration de `notify()`. **Zéro dépendance Engine/Broker.** |
| `notification` (struct) | lib (`notification_types.hh`) | **Donnée pure** d'un événement de notification émis (type, intervalle, contacts notifiés). Plus d'`execute()`, plus de `notifier*`. |
| `notification_callbacks` | lib | Interface abstraite vers l'application hôte, indexée par id. |
| `contact_viability` | lib | `should_notify_contact()` : filtre pur d'un contact (options, période, type accepté) sur snapshot `contact`. |
| `evaluate_escalations` | lib (`escalation.{hh,cc}`) | Fonction pure : viabilité + agrégation des escalades sur snapshots (partagée Engine/Broker). |
| `notification_types.hh` | lib | Enums + structs valeur `config`, `contact`, `resource_state`, `delivery_result`, `notification`. |
| `engine_notification_callbacks` | Engine (`cce_core`) | Implémentation : résout host/service par id, fournit l'état, et **porte la livraison** (sélection des contacts + macros + `notify_contact`). |
| `broker_notification_callbacks` | Broker (`broker/core`) | Implémentation côté Broker : lit l'état depuis `broker_cache`, décide, et dispatche l'exécution au poller via `pb_notification_execute`. |
| `notifier` | Engine | Ne stocke plus l'état de notification. Conserve `host_id()/service_id()` et **délègue** par id au manager. |

```mermaid
flowchart LR
    subgraph lib["Librairie notifications (sans dépendance Engine)"]
        NM["notification_manager<br/>(politique + état + notify())"]
        NEV["notification<br/>(donnée pure)"]
        CB["notification_callbacks<br/>(interface)"]
        NM --> NEV
        NM -. utilise .-> CB
    end
    subgraph engine["Engine (cce_core)"]
        NF["notifier / host / service"]
        ENC["engine_notification_callbacks<br/>(implémente l'interface)"]
    end
    NF -- "notify(...) délégué par id" --> NM
    ENC -. implémente .-> CB
    ENC -- "résout par id, lit l'état, livre" --> NF
```

### Injection du backend (load / unload)

L'application hôte injecte son implémentation **une fois**, comme pour les
downtimes :

```cpp
notifications::notification_manager::load(
    std::make_unique<engine_notification_callbacks>());
```

C'est fait dans `main.cc` (et dans le harnais de tests `engine/tests/helper.cc`)
à côté du `downtime_manager::load`. Au teardown, `notification_manager::unload()`
relâche le backend et vide l'état. Quand un `notifier` est détruit, son
destructeur appelle `forget(host_id, service_id)` pour purger l'état de cette
ressource (l'état ne vit plus avec l'objet, il faut donc le nettoyer
explicitement).

### Le pipeline `notify()`

`notifier::notify(...)` n'est plus qu'un délégateur : il convertit `this` en
`(host_id, service_id)` et appelle `notification_manager::notify(...)`. Toute la
logique vit dans le manager, qui ne touche jamais directement le notifier.

```mermaid
sequenceDiagram
    participant SRC as host/service ou callback
    participant NF as notifier::notify()
    participant NM as notification_manager::notify()
    participant CB as notification_callbacks (backend)

    SRC->>NF: notify(type, author, data, options)
    NF->>NM: notify(host_id, service_id, type, author, data, options)
    NM->>NM: cat = get_category(type)
    NM->>NM: is_notification_viable(host_id, service_id, cat, type, options)
    alt non viable
        NM-->>NF: OK (rien n'est envoyé)
    else viable
        NM->>NM: inc_notification_number (sauf recovery)
        NM->>NM: current_id = next_notification_id()
        NM->>NM: set_current_notification_id(current_id)
        NM->>NM: already = contacts de la notif normale courante
        NM->>CB: deliver(host_id, service_id, cat, type, id, number, author, message, options)
        CB-->>NM: { notified_contacts, notification_interval, escalated }
        NM->>NM: crée l'objet notification (donnée pure)
        NM->>NM: set_last_notification(now) si des contacts ont été notifiés
        NM->>NM: machine à états sur events[cat] (carry-forward normal, reset selon cat, reset du numéro sauf ack/downtime)
        NM-->>NF: OK
    end
    NF-->>SRC: retval
```

La machine à états est inchangée par rapport à l'ancienne version (un RECOVERY
efface `cat_normal` et `cat_recovery`, etc.) — seul son **lieu de stockage** a
changé : la map `(host_id, service_id) → notification_state` du manager au lieu
du tableau membre du `notifier`.

### La viabilité : un état-instantané, puis une fonction pure

La viabilité n'interroge plus le notifier méthode par méthode. Le manager
récupère **un instantané** (`resource_state`) et la **configuration globale**
(`global_config`) via le backend, puis la décision est une fonction **pure** de
ces valeurs (plus l'état interne du manager : notification courante, numéro).

```mermaid
sequenceDiagram
    participant NM as notification_manager
    participant CB as notification_callbacks (backend)
    participant ENG as Engine (host/service, config, timeperiod)

    NM->>CB: get_global_config()
    CB->>ENG: lit pb_indexed_config.state()
    CB-->>NM: global_config { enabled, interval_length, send_recovery_notifications_anyway }
    NM->>CB: get_state(host_id, service_id)
    CB->>ENG: résout la ressource par id, lit son état + période de notif (évaluée à l'heure courante, lue en interne)
    CB-->>NM: resource_state (flapping, downtime, hard_state, ack, current_state, notify_on, délais…)
    NM->>NM: _is_notification_viable_<cat>(resource_state, global_config, ...)
    note over NM: décision = fonction pure de (resource_state, global_config)<br/>+ état manager (current_notification, notification_number)
```

### La livraison : `deliver()`

La sélection des contacts (escalations, groupes) et l'envoi effectif (macros,
exécution des commandes via `notify_contact`) restent **côté Engine**, dans
`engine_notification_callbacks::deliver()`. Le manager fournit l'identité et les
paramètres ; le backend renvoie qui a été notifié et les paramètres ajustés par
escalation.

```mermaid
sequenceDiagram
    participant NM as notification_manager
    participant ENC as engine_notification_callbacks::deliver()
    participant NF as notifier (host/service)
    participant CT as contacts

    NM->>ENC: deliver(host_id, service_id, cat, type, id, number, author, message, options)
    ENC->>ENC: n = get_resource(host_id, service_id)
    ENC->>NF: get_contacts_to_notify(cat, type) → contacts + interval + escalated
    ENC->>NF: grab_macros_r(mac) + macros auteur/type/numéro/id
    loop pour chaque contact
        ENC->>NF: notify_contact(mac, contact, type, ...)
        NF->>CT: exécute la commande de notification
        ENC->>ENC: si OK, ajoute le contact aux notifiés
    end
    ENC-->>NM: delivery_result { notified_contacts, notification_interval, escalated }
```

> Pour le routage du **recovery**, la sélection (`get_contacts_to_notify` →
> `contact::should_be_notified`) consulte la notification normale précédente via
> les accesseurs du `notifier`, qui délèguent au manager par id. La boucle
> reste donc correcte sans que la librairie n'ait à connaître les contacts.

### Escalations : évaluation mutualisée

Engine et Broker évaluent tous deux les escalades d'une ressource, mais à partir
de sources de données différentes : Engine parcourt la liste `_escalations` du
`notifier` (objets à pointeurs), Broker interroge son `broker_cache`
(conteneurs `multi_index`). L'**algorithme**, lui, est strictement identique :
tester la viabilité de chaque escalation, retenir le plus petit intervalle et
unir les contactgroups des escalades viables.

Cette logique vit désormais **une seule fois**, dans
`common/notifications/escalation.{hh,cc}` :

```cpp
struct escalation {                    // snapshot d'une escalation
  uint32_t first_notification;         // plage du numéro de notif ...
  uint32_t last_notification;          // ... 0 = illimité
  uint32_t notification_interval;      // unités de config brutes
  uint32_t escalate_on;                // bitmask notification_flag
  bool in_period;                      // now ∈ période, PRÉCALCULÉ par l'hôte
  std::vector<std::string> contactgroups;
};

struct escalation_evaluation {         // résultat de l'évaluation
  bool escalated;
  uint32_t notification_interval;      // plus petit parmi les viables
  absl::btree_set<std::string> contactgroups;   // union des viables
};

escalation_evaluation evaluate_escalations(
    const std::vector<escalation>& escalations,
    notifier_type type,                // host ou service → array de flags
    int state, uint32_t notification_number);
```

Une escalation est **viable** quand `escalate_on` couvre l'état courant, que le
numéro de notification est dans `[first, last]` (`last == 0` = illimité) et que
`in_period` est vrai. La fonction ne dépend **d'aucune** timeperiod : le test de
période, dont la source (`escalation_period_ptr` côté Engine, map
`_timeperiods` côté Broker) et le fuseau diffèrent, est **précalculé** par
chaque hôte dans `in_period`. De même l'intervalle reste en unités de config
brutes : la fonction ne fait que des `min` (indépendants de l'unité), chaque
côté multipliant ensuite par son propre `interval_length`.

#### Côté Engine — `notifier::get_contacts_to_notify()`

```mermaid
sequenceDiagram
    participant GC as notifier::get_contacts_to_notify()
    participant LIB as evaluate_escalations()  (common/notifications)
    participant CG as contactgroups (résolution locale)

    GC->>GC: pour chaque escalation de _escalations :<br/>snapshot { first,last,interval,escalate_on }
    GC->>GC: in_period = période vide OU escalation_period_ptr->check_time_against_period(now)
    GC->>GC: mémorise une map locale nom → contactgroup*
    GC->>LIB: evaluate_escalations(snapshots, type, state, number)
    LIB-->>GC: { escalated, notification_interval, contactgroups }
    alt escalated
        GC->>CG: pour chaque contactgroup viable → ses membres
        CG->>GC: contact retenu si should_be_notified(cat, type)
    else non escaladé
        GC->>CG: contacts directs + contactgroups du notifier<br/>(même filtre should_be_notified)
    end
    GC-->>GC: set de contacts + notification_interval + escalated
```

#### Côté Broker — `broker_cache::notification_escalation()`

```mermaid
sequenceDiagram
    participant NE as broker_cache::notification_escalation()
    participant MI as multi_index (_host/_service_escalations)
    participant LIB as evaluate_escalations()  (common/notifications)
    participant CG as _contactgroups (résolution locale)

    NE->>MI: equal_range(host_id[, service_id])
    MI-->>NE: escalations de la ressource
    NE->>NE: pour chacune : snapshot { first,last,interval,escalate_on,contactgroups }
    NE->>NE: in_period = période vide OU _timeperiods[period]->check_time_against_period(now, tz)
    NE->>LIB: evaluate_escalations(snapshots, type, state, number)
    LIB-->>NE: { escalated, notification_interval, contactgroups }
    NE->>CG: pour chaque contactgroup viable → ses membres
    CG-->>NE: escalation_result { escalated, notification_interval, contact_names }
```

Ce qui **reste propre à chaque côté** : la résolution des contactgroups en
contacts (Engine filtre par `should_be_notified` et gère le fallback
non-escaladé ; Broker retourne les noms des membres, filtrés plus loin par
`should_notify_contact`) et le calcul de `in_period`.

### Points clés

- L'état runtime de notification (numéro, ids, horodatages, et les six
  événements `notification`) vit dans le manager, **indexé par
  `(host_id, service_id)`** — ce qui le rend persistable / centralisable (vers
  Broker) sans changer l'API.
- `notification_manager`, `notification` et l'interface **ne référencent aucun
  type d'Engine** ; la librairie loggue via `common/log_v2`
  (catégories `FUNCTIONS` / `NOTIFICATIONS`).
- Le couplage à l'application hôte est concentré dans les implémentations du
  backend, injectées au démarrage : `engine_notification_callbacks` (Engine) et
  `broker_notification_callbacks` (Broker en `notification_mode = broker`).
- Le destructeur de `notifier` appelle `forget(host_id, service_id)` ; sans cela
  l'état fuirait dans la map globale.

### Fichiers (code)

Librairie partagée (`common/notifications/`, aucune dépendance Engine/Broker) :

- `notification_manager.{hh,cc}` — politique, état, `notify()`, viabilité.
- `notification_callbacks.hh` — l'interface injectée.
- `contact_viability.{hh,cc}` — `should_notify_contact()` (filtre pur d'un
  contact).
- `escalation.{hh,cc}` — `evaluate_escalations()` (viabilité + agrégation des
  escalades).
- `notification_types.hh` — enums + structs valeur (`config`, `contact`,
  `resource_state`, `delivery_result`, `notification`, `escalation`…).

Implémentations hôtes (le seul couplage) :

- `engine/src/engine_notification_callbacks.{hh,cc}` — backend Engine
  (résolution par id, livraison locale via `notify_contact`).
- `engine/src/notifier.cc` — délégateurs par id, `get_contacts_to_notify()`
  (appelle `evaluate_escalations`), `~notifier` → `forget`.
- `broker/core/src/broker_notification_callbacks.cc` — backend Broker (décision
  puis dispatch `pb_notification_execute`).
- `broker/core/cache/broker_cache.cc` — `notification_escalation()` (appelle
  `evaluate_escalations`).

