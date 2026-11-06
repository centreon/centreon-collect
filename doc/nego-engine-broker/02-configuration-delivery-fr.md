# Lecture de la configuration Engine

[← Précédent](01-negotiation-fr.md) · [Index](README-fr.md) · [Suivant →](03-configuration-validation-fr.md)

<!-- TOC -->
* [Gestion de la configuration côté Engine](#gestion-de-la-configuration-côté-engine)
* [Gestion de l’envoi de la configuration par Broker à Engine](#gestion-de-lenvoi-de-la-configuration-par-broker-à-engine)
  * [Le parcours d'une configuration](#le-parcours-dune-configuration)
  * [L'annonce](#lannonce)
  * [La préparation](#la-préparation)
  * [La livraison](#la-livraison)
  * [Le tour de configuration](#le-tour-de-configuration)
    * [Un export qui arrive pendant un tour ouvert](#un-export-qui-arrive-pendant-un-tour-ouvert)
* [Quelques points plus techniques](#quelques-points-plus-techniques)
  * [Surveillance du répertoire de cache](#surveillance-du-répertoire-de-cache)
    * [Les rouages](#les-rouages)
    * [Quels événements font foi](#quels-événements-font-foi)
    * [Le scan de secours](#le-scan-de-secours)
    * [Perte du watch `inotify`](#perte-du-watch-inotify)
    * [Démarrage de la surveillance](#démarrage-de-la-surveillance)
* [Split de broker::config::applier::state](#split-de-brokerconfigapplierstate)
  * [Cycle de vie de broker_state, du cache et du downtime_manager](#cycle-de-vie-de-broker_state-du-cache-et-du-downtime_manager)
    * [Démarrage](#démarrage)
    * [Barrière de readiness au démarrage](#barrière-de-readiness-au-démarrage)
    * [Arrêt](#arrêt)
<!-- TOC -->

## Gestion de la configuration côté Engine

`Engine` est démarré avec la nouvelle configuration. Il lit la
configuration sérialisée Protobuf disponible dans son `HOME`. Il se
connecte ensuite à `Broker` et envoie le message `Welcome` avec le bon
numéro de version de sa configuration. On a vu que `Broker` répond juste
en disant s'il supporte ou non la configuration centralisée. Dans un second temps,
Broker, en tâche de fond, vérifie si des nouvelles configurations sont disponibles
pour `Engine`. S'il s'en présente une, il
envoie un message avec la différence de configuration.
`cbmod` le garde en attente et lorsqu’`Engine` recommence un cycle dans
sa boucle principale, il applique cette configuration.

S’il y a de la rétention, elle est envoyée en tâche de fond pendant
l’application de la nouvelle configuration.
Une fois la rétention terminée, `Engine` ayant appliqué la configuration,
dans le cas d'un reload, envoie un message
`InstanceConfiguration` à `Broker` pour lui dire qu’il a bien reçu la
configuration.

FIXME DBO: Le message `InstanceConfiguration` est-il toujours nécessaire ?

Mais en fait, il y a un message spécifique pour cela, c'est le message
`pb_diff_state_ack` qui est envoyé par `Engine` à `Broker` pour lui dire
qu’il a bien reçu et appliqué la nouvelle configuration.

## Gestion de l’envoi de la configuration par Broker à Engine

Cette section suit une configuration depuis son écriture par PHP jusqu'à son arrivée en base de données. La surveillance du répertoire de cache (`inotify`, strands, reprise du watch, scan de secours) est décrite à part, dans [Surveillance du répertoire de cache](#surveillance-du-répertoire-de-cache).

### Le parcours d'une configuration

Quatre fichiers jalonnent ce parcours, et chacun ne dit qu'une chose :

| Fichier | Répertoire | Ce qu'il dit | Écrit par | Disparaît |
|---|---|---|---|---|
| `X.lck` ou `pollers.lck` | cache PHP | une configuration est arrivée, `Broker` ne l'a pas encore lue | PHP | supprimé par `Broker` dès que `new-X.prot` est écrit |
| `new-X.prot` | `pollers-configuration` | une configuration est prête et attend l'acquittement du poller X | le cycle de `Broker` | renommé en `X.prot` à l'acquittement |
| `diff-X.prot` | `pollers-configuration` | la différence entre `X.prot` et `new-X.prot`, ou la configuration complète s'il n'y a pas de `X.prot` | le cycle de `Broker` | supprimé à son intégration dans le diff global |
| `X.prot` | `pollers-configuration` | la dernière configuration acquittée par le poller X | `Broker`, par renommage | remplacé au renommage suivant |

Les étapes :

1. **L'annonce.** PHP écrit la configuration du poller X dans le répertoire `X/`, puis crée `X.lck`, ou un `pollers.lck` qui nomme tous les pollers de l'export.
2. **La préparation.** `Broker` lit et valide la configuration, écrit `new-X.prot` puis `diff-X.prot`, et supprime l'annonce.
3. **La livraison.** Le stream BBDO du poller X lui envoie un `DiffState` lu dans `diff-X.prot`, ou dans `new-X.prot` si `Engine` ne peut pas appliquer la différence.
4. **L'acquittement.** `Engine` applique la configuration et répond par un `pb_diff_state_ack`. `Broker` renomme `new-X.prot` en `X.prot` et applique `diff-X.prot` au cache.
5. **La fermeture du tour.** Quand tous les pollers servis ont acquitté, `Broker` fusionne les `diff-*.prot` en un diff global, le publie vers `unified_sql` et le cache, puis supprime les `diff-*.prot`.

```mermaid
sequenceDiagram
    participant PHP
    participant C as Répertoire de cache
    participant B as Broker
    participant P as pollers-configuration
    participant E as Engine X
    participant DB as unified_sql et cache

    PHP ->> C: écrit X/ puis X.lck
    C -->> B: événement inotify, regroupement de 500 ms
    B ->> B: lecture et validation
    B ->> P: écrit new-X.prot puis diff-X.prot
    B ->> C: supprime X.lck
    B ->> P: lit diff-X.prot, ou new-X.prot
    B ->> E: DiffState
    E -->> B: pb_diff_state_ack
    B ->> P: renomme new-X.prot en X.prot
    B ->> DB: diff-X.prot appliqué au cache
    Note over B,P: le tour se ferme quand tous les pollers servis ont acquitté
    B ->> DB: diff global, fusion des diff-*.prot
    B ->> P: supprime les diff-*.prot
```

### L'annonce

PHP dispose de deux formes d'annonce, et `Broker` lit les deux.

| | `<ID>.lck` | `pollers.lck` |
|---|---|---|
| Contenu | vide, seul le nom compte | les pollers de l'export, un identifiant par ligne |
| Délimitation de l'export | devinée par `Broker` | donnée par le fichier |
| Regroupement | 500 ms sans nouvel événement, plafond de 5 s | aucun |
| Qui poste le travail | `_post_individual_work()` | `_post_batch_work()` |
| Suppression | une par une, à mesure que les `new-<ID>.prot` sont écrits | en fin de cycle |

`pollers.lck` est la forme cible. Avec des `<ID>.lck`, `Broker` ne peut que regrouper ce qui arrive de façon rapprochée. Or l'intervalle entre deux `.lck` est le temps de génération de la configuration suivante, qui dépasse le délai de regroupement sur un gros poller : l'export se coupe alors en plusieurs lots. Un objet déplacé d'un poller vers un autre est vu comme un retrait puis un ajout dans deux tours successifs, au lieu d'une modification dans un seul. Le contrat côté PHP est décrit dans [php-evolutions](../php-evolutions-fr.md#évolution-3--un-lot-de-configuration-explicite--pollerslck).

Le regroupement repousse l'échéance de 500 ms à chaque nouvel événement. Une rafale de `.lck`, comme un déploiement de masse, donne ainsi un seul lot, et les configurations stockées ne sont relues qu'une fois. Le plafond de 5 s empêche un flux continu d'événements de repousser le traitement indéfiniment.

Une annonce ne fait qu'annoncer. Elle disparaît dès que `Broker` a préparé ce qu'elle annonce, que le poller soit connecté ou non. Pour PHP, sa disparition signifie « `Broker` a pris ta configuration en charge », et non « elle a atteint son poller » : un poller éteint ne pouvant rien acquitter, c'est tout ce que PHP peut utilement attendre. C'est `new-<ID>.prot`, dans le répertoire de `Broker`, qui marque une livraison en attente. Un `pollers.lck` ne pourrait d'ailleurs pas tenir ce rôle, puisque les pollers qu'il nomme n'acquittent pas en même temps.

### La préparation

Le cycle, `_run_config_cycle()`, reçoit son lot en paramètre et traite chaque poller en deux temps. Un échec n'a pas le même sens dans l'un et dans l'autre.

**Lire et valider** (`_read_poller_conf()`) : `hash_directory` calcule la version de la configuration, puis `parse`, `expand` et `resolve` la chargent avec la librairie `engine_conf`, et la [validation inter-pollers](03-configuration-validation-fr.md#la-validation-inter-pollers--load_foreign_objects) la vérifie. Un échec de validation est une propriété du contenu poussé, qui ne redeviendra pas valide seul : la configuration est refusée et l'annonce consommée. PHP en créera une nouvelle avec la configuration corrigée.

**Stocker** (`_store_poller_conf()`, puis `_prepare_diff_for_poller()`) : écriture de `new-X.prot`, puis de `diff-X.prot`. Un échec ici est affaire de disque et ne dit rien de la configuration : l'annonce est gardée, et le cycle suivant relit les sources.

| Sortie | Annonce | Compté |
|---|---|---|
| `hash_directory` échoue | gardée | — |
| `build_test_file` échoue | gardée | — |
| validation en erreur | consommée | `refused` |
| stockage impossible | gardée | — |
| succès | consommée | `ready` |

L'annonce n'est supprimée qu'une fois `new-X.prot` écrit. À aucun instant il n'y a ni l'un ni l'autre : un arrêt brutal entre les deux laisse toujours de quoi recommencer.

⚠️ Un `parse` qui échouerait pour une raison d'entrée-sortie serait compté comme une configuration invalide. C'est tenable, puisque `centengine.test` vient d'être écrit, mais une garantie franche demanderait un type d'exception distinct côté parseur.

**Quelle différence.** `diff-X.prot` est toujours calculé depuis `X.prot` quand il existe, même si `Engine` tourne avec une autre version, parce qu'il a perdu son `state.prot` par exemple. La base et le cache reflètent `X.prot`, et une configuration complète leur ferait tout ajouter sans rien retirer. `diff-X.prot` note la version dont il part (`from_version`), et c'est à l'envoi que l'on décide si `Engine` peut l'appliquer. Sans `X.prot`, `diff-X.prot` contient la configuration complète.

Un poller qui tourne déjà la version préparée ne reçoit rien. Un poller non connecté n'a pas de `diff-X.prot` : son `new-X.prot` l'attend jusqu'à sa connexion.

Chaque cycle qui a travaillé se termine par une ligne de bilan, qui nomme les pollers dont la configuration est prête mais qui ne sont pas connectés :

```
Configuration cycle: 3 ready, 1 sent, 0 refused, 2 poller(s) not connected: 2, 3
```

### La livraison

Le stream BBDO du poller X vérifie à chaque lecture si une configuration lui est due (`poller_needs_update()`). Il faut pour cela :

* qu'une configuration soit préparée pour lui (`available_conf`), différente de celle qu'il fait tourner (`engine_conf`), et pas encore remise ;
* qu'aucune autre configuration ne soit en cours pour lui, c'est-à-dire remise et pas encore acquittée (`conf_in_flight`).

La seconde condition vient de ce qu'`Engine` applique les `DiffState` dans l'ordre où il les reçoit, sans vérifier la version dont ils partent. Tant que C1 n'est pas acquitté, `X.prot` vaut encore C0 : une configuration C2 préparée entre-temps est une différence C0→C2, qu'`Engine` appliquerait par-dessus C1. L'attente n'est pas bornée dans le temps : `Engine` répond toujours, que la configuration ait été appliquée ou non (voir l'échec plus bas), et un poller qui meurt en route se déconnecte, ce qui oublie la configuration en cours.

**Différence ou configuration complète.** Le stream lit `diff-X.prot`. Si `Engine` ne tourne pas avec la version dont part la différence (`from_version`), il ne peut pas l'appliquer : le stream lit alors `new-X.prot` et envoie la configuration complète. Le choix se fait à l'envoi et non à la préparation, parce qu'`Engine` a pu se reconnecter avec une autre version entre-temps.

Côté `Engine`, le `DiffState` est stocké par `cbmod` et appliqué dans la boucle principale. `Engine` écrit alors son état complet dans `state.prot` et répond par un `pb_diff_state_ack` qui porte la version appliquée. S'il n'arrive pas à l'appliquer, il restaure sa configuration précédente et répond aussi, par un `pb_diff_state_ack` qui porte la version qu'il fait toujours tourner, la version refusée (`failed_version`) et la raison (`error`).

**L'acquittement** (`on_poller_conf_acknowledged()`). La version acquittée devient `engine_conf`, et la configuration en cours est libérée. Si c'est la version préparée et remise :

1. `new-X.prot` est renommé en `X.prot` ;
2. `diff-X.prot` est appliqué au cache ;
3. si c'était le dernier acquittement attendu, le [tour](#le-tour-de-configuration) est fermé.

Si c'est une version dépassée, parce qu'une configuration plus récente a été préparée entre-temps, rien n'est renommé ni appliqué : `new-X.prot` et `diff-X.prot` contiennent déjà la suivante, qui part à la lecture suivante du stream.

**L'échec** (`on_poller_conf_rejected()`). La version que le poller fait toujours tourner devient `engine_conf`, et la configuration en cours est libérée. Si la version refusée est la version préparée :

1. `new-X.prot` et `diff-X.prot` sont supprimés : ils décrivent une configuration que le poller ne fera pas tourner, et la fermeture du tour porterait sinon ce diff en base ;
2. le poller sort du tour, qui peut alors se fermer pour les autres ;
3. la version refusée et la raison sont gardées (`rejected_conf`, `rejection_error`), journalisées en erreur et visibles par la RPC `GetPollers`. Cette version n'est plus renvoyée, elle échouerait de la même façon.

Le prochain export repart de `X.prot`, que le poller fait bien tourner. Une reconnexion du poller, qui recrée son entrée, retente aussi la version refusée : l'erreur pouvait venir de la machine plutôt que de la configuration.

Si la version refusée est dépassée, rien n'est supprimé : `new-X.prot` et `diff-X.prot` contiennent déjà la suivante, qui part aussitôt.

**La livraison à la connexion d'un poller.** Quand un poller absent lors du lot se connecte, `add_peer()` regarde ce qui l'attend, dans cet ordre :

* **une annonce `X.lck`** : elle n'a pas encore été lue, le poller passe par un cycle complet ;
* **sinon un `new-X.prot`** : la configuration est prête, `_prepare_diff_from_new_prot_file()` calcule la différence avec `X.prot` sans relire les sources ;
* **ni l'un ni l'autre** : il n'y a rien à livrer. Si `Broker` n'a pas non plus `X.prot`, il demande sa configuration au poller (`DiffState{unknown=true}`, voir [Mise en pratique](03-configuration-validation-fr.md#mise-en-pratique)).

L'ordre découle du passage de témoin : le cycle supprime l'annonce dès qu'il a écrit `new-X.prot`, donc une annonce encore présente porte forcément sur une poussée postérieure au `new-X.prot` posé à côté d'elle.

### Le tour de configuration

Un tour regroupe les pollers servis par un même export. Le fermer déclenche le diff global : `_publish_global_diff()` fusionne les `diff-*.prot` en un `pb_global_diff_state`, le publie vers `unified_sql` et le cache, puis supprime les fichiers. La base n'est donc mise à jour qu'une fois tous les pollers de l'export servis, ce qui permet de réconcilier en une seule modification un objet déplacé d'un poller à un autre.

Chaque pair porte sa place dans le tour (`conf_round`) : `out` s'il n'en fait pas partie, `sent` quand sa configuration lui a été remise, `acknowledged` quand il l'a acquittée. `try_close_conf_round()` ferme le tour quand tous les pairs `sent` sont passés à `acknowledged` et qu'aucun pair n'attend encore sa configuration (`_peer_owes_conf()`), qu'elle puisse partir tout de suite ou qu'elle attende l'acquittement d'une précédente. Le test et la remise de tous les pairs à `out` se font sous le même verrou : deux acquittements simultanés ne peuvent pas publier deux fois le même tour.

```
All engine peers acknowledged? 1/3 acknowledged, 0 still waiting to be sent
```

Quatre règles tiennent ce mécanisme :

* **Un tour ne se ferme pas tant qu'un poller attend sa configuration.** Le diff global consomme tous les `diff-*.prot` du répertoire, y compris celui d'un poller pas encore servi. Ce fichier ne serait jamais réécrit, et le poller ne recevrait jamais sa configuration.
* **« Attend sa configuration » se teste par `available_conf != engine_conf`**, et non par `available_conf` non vide : ce champ n'est jamais vidé, et tous les pollers paraîtraient en attente pour toujours.
* **Préparer une version déjà remise ne sort pas le poller du tour.** PHP peut annoncer deux fois le même export. Remettre le poller à `out` pour la même version perdrait son acquittement : `try_close_conf_round()` ne le compterait plus, et le tour ne se fermerait jamais. `_prepare_diff_for_poller()` ne touche donc à `conf_round` que si la version change.
* **Un poller non connecté n'entre pas dans le tour.** Rien n'est préparé pour lui, il n'a donc aucun `diff-X.prot` à perdre. Un `1/1 acknowledged` sur une plateforme dont deux pollers sont éteints est normal, la ligne de bilan du cycle nommant les absents. Il en va de même d'une entrée connue du seul `topology.cache` (voir [Un poller derrière un relais se comporte comme un poller direct](06-remote-servers-fr.md#un-poller-derrière-un-relais-se-comporte-comme-un-poller-direct)).

#### Un export qui arrive pendant un tour ouvert

Un export C2 peut arriver alors que le tour de l'export C1 n'est pas fermé : certains pollers ont acquitté C1, d'autres non. Pour un poller qui a acquitté, `X.prot` vaut déjà C1, et `diff-X.prot`, la différence C0→C1, attend la fermeture du tour pour entrer dans le diff global. Préparer C2 calcule la différence depuis `X.prot`, soit C1→C2, et écrase `diff-X.prot` : la base et le cache ne reçoivent jamais C1.

```mermaid
sequenceDiagram
    participant PHP
    participant B as Broker
    participant E1 as Engine 1
    participant E3 as Engine 3
    participant DB as unified_sql et cache

    PHP ->> B: export C1 (pollers 1 à 4)
    B ->> E1: DiffState C1
    E1 -->> B: ack C1
    Note over B: 1.prot = C1<br/>diff-1.prot = C0→C1 attend la fermeture du tour
    Note over E3: pas encore servi
    PHP ->> B: export C2
    Note over B: sans précaution, diff-1.prot serait écrasé par C1→C2<br/>et C1 ne serait jamais intégré
    E3 -->> B: ack, le tour se ferme
    B ->> DB: diff global, C1→C2 pour le poller 1
    Note over DB: n'a jamais reçu C1
```

Avant de stocker une nouvelle configuration pour un poller, le cycle regarde donc si son `diff-X.prot` attend l'intégration : le fichier existe, et `new-X.prot` n'existe plus puisqu'il a été renommé à l'acquittement. Si c'est le cas, `_integrate_acknowledged_diffs()` publie aussitôt un diff global partiel avec les `diff-*.prot` des pollers qui ont acquitté, et les sort du tour (`leave_conf_round()`). Le tour continue avec les autres.

Cet état se lit sur les fichiers plutôt que sur `conf_round` : une reconnexion recrée l'entrée du poller et remet `conf_round` à `out`, alors que les fichiers restent.

Les pollers qui n'ont pas encore acquitté C1 suivent les règles de [La livraison](#la-livraison) : rien ne leur est envoyé tant que C1 est en cours, et leur acquittement de C1, qui n'est plus la version préparée, ne renomme rien. Ils reçoivent ensuite C2 en entier, puisqu'ils tournent C1 alors que `diff-X.prot` part de C0.

```mermaid
sequenceDiagram
    participant PHP
    participant B as Broker
    participant E1 as Engine 1 (C1 acquitté)
    participant E3 as Engine 3 (C1 en cours)
    participant DB as unified_sql et cache

    PHP ->> B: export C1
    B ->> E1: DiffState C1
    B ->> E3: DiffState C1
    E1 -->> B: ack C1
    PHP ->> B: export C2
    B ->> DB: diff global partiel, C0→C1 pour le poller 1
    Note over B: diff-1.prot = C1→C2<br/>diff-3.prot = C0→C2
    B ->> E1: DiffState C1→C2
    Note over B,E3: rien n'est envoyé à 3 tant que C1 est en cours
    E3 -->> B: ack C1, plus la version préparée
    Note over B: engine_conf(3) = C1<br/>ni renommage, ni cache
    B ->> E3: C2 en entier (C1 ≠ from_version C0)
    E1 -->> B: ack C2
    E3 -->> B: ack C2
    B ->> DB: diff global, C1→C2 (poller 1) et C0→C2 (poller 3)
```

Le prix à payer est l'atomicité de l'export : la base reçoit C1 pour les pollers qui l'ont acquitté, puis C2 pour tous. Un objet déplacé entre un poller qui a acquitté C1 et un poller qui ne l'a pas fait peut donc manquer ou apparaître en double le temps d'un tour.

Toutes les opérations sur les fichiers d'un poller se font sous un même verrou, `_conf_handover_m` : le stockage et la préparation par le cycle, l'acquittement, la préparation à la connexion, la réponse à un relais et le retrait d'un poller. Ce verrou ne couvre ni la lecture ni la validation des sources, qui restent la partie coûteuse du cycle.

## Quelques points plus techniques

Lors de la connexion d'un Engine au Broker, la méthode `config::applier::add_peer()` est appelée. Elle fonctionne
de la façon suivante :

```mermaid
stateDiagram-v2
    [*] --> config/applier/add_peer()
    config/applier/add_peer() --> peer_already_known?
    state if_state <<choice>>
    peer_already_known? --> if_state
    if_state --> peer_is_updated:True
    if_state --> peer_is_added:False
    state extended_nego <<choice>>
    peer_is_updated --> extended_nego
    peer_is_added --> extended_nego
    extended_nego --> [*]: !extended_negotiation || peer is not a Broker
    state if_watch_engine_timer_started <<choice>>
    extended_nego --> if_watch_engine_timer_started: extended_negotiation && peer is a Broker
    if_watch_engine_timer_started --> start_watch_engine_conf_timer(): engine conf timer not started
    start_watch_engine_conf_timer() --> prot_file_created_from_state
    if_watch_engine_timer_started --> prot_file_created_from_state: engine conf timer already started

    state if_existing_lck_file_for_current_poller_id <<choice>>
    prot_file_created_from_state --> if_existing_lck_file_for_current_poller_id
    if_existing_lck_file_for_current_poller_id --> keep_lck_file_in_memory: Un fichier lck existe pour ce poller ID
    if_existing_lck_file_for_current_poller_id --> [*]: Aucun fichier lck pour ce poller ID
    keep_lck_file_in_memory: Ajout de l'ID dans la liste à traiter
    keep_lck_file_in_memory --> [*]
```

Cette méthode mémorise le poller dans sa liste de peers. Elle démarre le timer de surveillance de la configuration
`Engine` si la négociation est étendue et que le peer est un `Engine`. Et elle vérifie aussi s'il existe déjà un
fichier `.lck` pour le poller ID car `inotify` ne remonte pas les fichiers existant avant son démarrage. Si un tel
fichier est trouvé, l'ID du poller est ajouté à la liste à traiter pour le prochain tick du timer.

Par la suite, c'est `inotify` qui signale l'arrivée d'une annonce ; la liste des
pollers à traiter est enrichie puis traitée en tâche de fond, après un regroupement
de 500 ms pour les annonces individuelles ou immédiatement pour un `pollers.lck`.
Un filet de sécurité à 5 min couvre ce que les événements ne peuvent pas dire.
Intéressons-nous à cette partie.

Le travail est exécuté par `config::applier::state::_run_config_cycle()`, posté par
`_post_individual_work()` ou `_post_batch_work()` selon la forme de l'annonce.

```mermaid
stateDiagram-v2
    [*] --> watch_engine_conf
    watch_engine_conf: _watch_engine_conf(poller_ids)
    watch_engine_conf --> pour_chaque_poller_id
    note right of watch_engine_conf: Cette fonction récupère les IDs remontés par inotify<br/>(complétés par le scan de secours du répertoire).<br/>Le fichier ID.lck n'est PAS supprimé ici, car lire un<br/>événement n'est pas traiter ce qu'il annonce.<br/>C'est le cycle qui le supprime, une fois new-ID.prot écrit.
    pour_chaque_poller_id: Pour chaque poller ID dans poller_ids
    state garde_attente <<choice>>
    pour_chaque_poller_id --> garde_attente
    garde_attente --> fin_pour_chaque_poller_id: Poller non connecté ET new-ID.prot<br/>déjà présent → on garde le .lck<br/>et on attend la connexion
    lect_conf_engine: Lecture de la configuration Engine
    garde_attente --> lect_conf_engine: sinon
    lect_conf_engine --> resolution_de_la_configuration_engine
    resolution_de_la_configuration_engine: Resolution et extension de la configuration Engine
    resolution_de_la_configuration_engine --> ecriture_dans_fichier__new_ID_prot
    ecriture_dans_fichier__new_ID_prot: Ecriture de la configuration au format State<br/> dans un fichier new-ID.prot
    ecriture_dans_fichier__new_ID_prot --> preparation_de_la_difference
    preparation_de_la_difference: _prepare_diff_for_poller(poller_id, state)
    note right of preparation_de_la_difference: Cette fonction prépare la différence<br/> entre la configuration actuelle et la nouvelle.<br/>Elle crée le fichier diff-ID.prot.
    preparation_de_la_difference --> suppression_conditionnelle_lck
    suppression_conditionnelle_lck: Le poller est-il connecté ?
    state lck_decision <<choice>>
    suppression_conditionnelle_lck --> lck_decision
    suppr_lck: Suppression du fichier ID.lck
    garde_lck: On garde ID.lck<br/>(rejeu à la connexion du poller)
    lck_decision --> suppr_lck: connecté (_is_engine_peer_connected)
    lck_decision --> garde_lck: non connecté
    suppr_lck --> fin_pour_chaque_poller_id
    garde_lck --> fin_pour_chaque_poller_id
    fin_pour_chaque_poller_id: Fin de boucle sur les poller IDs
    fin_pour_chaque_poller_id --> pour_chaque_poller_id
    fin_pour_chaque_poller_id --> [*]
```

Détaillons un peu le comportement de la méthode `config::applier::state::_prepare_diff_for_poller()`.

```mermaid
stateDiagram-v2
    prepare_diff_poller: _prepare_diff_for_poller(poller_id, state)
    [*] --> prepare_diff_poller
    recup_poller_in_peers: Récupération des informations sur<br/> le poller dans la liste des peers
    state si_pas_trouve <<choice>>
    recup_poller_in_peers --> si_pas_trouve
    prepare_diff_poller --> recup_poller_in_peers
    si_pas_trouve --> [*]: Poller non trouvé dans<br/> la liste des peers
    comparaison: Y a-t-il une différence entre<br/> la configuration actuelle et la nouvelle?
    si_pas_trouve --> comparaison: Poller trouvé dans<br/> la liste des peers
    state si_differe <<choice>>
    comparaison --> si_differe
    create_diff: Création d'un objet DiffState avec les différences
    si_differe --> create_diff: True
    sauv_diff: Sauvegarde de l'objet DiffState<br/> dans un fichier diff-ID.prot
    create_diff --> sauv_diff
    sauv_diff --> [*]
    si_differe --> [*]: False
```

Par conséquent, côté `state`, Broker surveille les configurations `Engine` et dès qu'une nouvelle est disponible,
il calcule la différence et crée les fichiers `diff-ID.prot` et `new-ID.prot`.

Chaque stream BBDO connaît son interlocuteur. Si `Broker` est connecté à `Engine`, un stream BBDO les relie et `Broker`
sait qu'il est connecté à un `Engine`, il connaît son poller ID, s'il supporte la configuration centralisée, etc...

Ce stream lit régulièrement les événements entrants, c'est de cette manière que `Broker` reçoit la supervision.
La lecture est faite par une méthode `bbdo::stream::read()`. C'est dans cette méthode que certaines actions comme
l'acquittement d'événements est faite. Et c'est aussi là que Broker détecte si une nouvelle configuration est disponible
pour le poller connecté. Dans le cas où une configuration est disponible, elle est alors lue et envoyée à `Engine`.

Un algorithme simplifié de cette méthode est le suivant :
```mermaid
stateDiagram-v2
    read: read(d, deadline)
    read_one_event: Lecture d'un événement BBDO
    read --> read_one_event
    state if_bbdo_event <<choice>>
    read_one_event --> if_bbdo_event
    if_bbdo_event --> handle_bbdo_event(d): Sa catégorie est BBDO
    state if_ack_needed <<choice>>
    if_bbdo_event --> if_ack_needed: Sa catégorie n'est pas BBDO
    handle_bbdo_event(d) --> read_one_event
    if_ack_needed --> send_event_acknowledgement(): Le nombre max d'événements a été atteint,<br/>envoi d'un acquittement.
    state if_peer_is_engine <<choice>>
    if_ack_needed --> if_peer_is_engine: sinon
    send_event_acknowledgement() --> if_peer_is_engine
    read_conf: Lecture de la différence de configuration<br/> Engine disponible diff-ID.prot
    if_peer_is_engine --> read_conf: Le peer est un Engine et<br/> une nouvelle configuration est disponible
    if_peer_is_engine --> [*]: sinon
    send_conf: Envoi de la différence de configuration<br/> Engine au poller
    read_conf --> send_conf
    update_peers: Mise à jour de la liste des peers<br/> pour signifier que le poller a reçu<br/>une mise à jour de sa configuration
    send_conf --> update_peers
    update_peers --> [*]
```

Dans cette même méthode `read()`, on a un appel à la méthode `handle_bbdo_event()` pour les événements de catégorie BBDO.
C'est dans cette méthode qu'on trouve entre autre le traitement de l'acquittement de la configuration par `Engine`.

Lorsque l'acquittement est reçu pour un poller d'ID poller_id, la liste des peers est mise à jour pour signifier que
ce poller est bien à jour. Le fichier `new-ID.prot` remplace le fichier `ID.prot`.

Enfin, lorsque tous les pollers sont à jour, un objet `com::centreon::engine::configuration::indexeed_diff_state`
est construit en mergeant tous les différentiels de pollers. Cet objet est publié par `Broker` pour être
traité par le stream `unifie_sql`.

L'agencement de ces fonctions donne à peu près le diagramme suivant :

```mermaid
sequenceDiagram
participant S as State Applier
participant BBDO as BBDO Stream
participant USQL as Unified SQL Stream

par Côté State applier
    par Thread principal de Broker
    S ->> S: add_peer
    note right of S: Ajout d'un nouveau poller.<br/>Démarrage du timer inotify si nécessaire.
    and Worker de configuration
        loop À chaque annonce (regroupée), ou 5 min
            S ->> S: _run_config_cycle(lot)
            note right of S: Lot venu des X.lck regroupés<br/>ou d'un pollers.lck lu d'un coup.<br/>Préparation des new-ID.prot et diff-ID.prot
        end
    end
and Lecture sur le stream BBDO
    BBDO ->> BBDO: read()
    activate BBDO
    BBDO ->> BBDO: lecture d'événements
    BBDO ->> BBDO: traitement des événements BBDO
    alt Cas d'un acquittement de configuration reçu
        BBDO ->> BBDO: mise à jour de la liste des peers<br/>pour signifier que le poller est à jour.
        alt Tous les pollers sont à jour
            BBDO ->> BBDO: préparation du diff global
            BBDO ->> BBDO: suppression des fichiers diff-ID.prot
            BBDO ->> USQL: Envoi du diff global
        end
    end
    alt Nouvelle configuration disponible pour le poller
    BBDO ->> BBDO: lecture du fichier diff-ID.prot
    BBDO ->> BBDO: envoi de la différence de configuration<br/>au poller
    BBDO ->> S: Mise à jour de la liste des peers<br/>pour ne pas l'envoyer une seconde fois.
    end
    deactivate BBDO
and Lecture sur le stream Unified SQL
    USQL ->> USQL: read()
    activate USQL
    USQL ->> USQL: lecture d'événements
    alt Cas d'un diff global reçu
        USQL ->> USQL: mise à jour de la base de données<br/>à partir du diff global
    end
    deactivate USQL
end
```

### Surveillance du répertoire de cache

Cette partie détaille comment `Broker` détecte les annonces décrites dans [L'annonce](#lannonce).

La surveillance est événementielle : une attente asynchrone (`async_wait_readable`) est posée sur le descripteur `inotify`, et rien ne s'exécute tant que le noyau n'a rien à dire. Au réveil, `Broker` lit les événements, réarme aussitôt l'attente, puis arme le délai de regroupement.

#### Les rouages

Quatre choses déclenchent un cycle : un événement `inotify`, l'échéance du regroupement, celle du filet de sécurité, et la connexion d'un poller. Elles se répartissent sur deux strands. `_watch_strand` surveille et ne travaille jamais, `_config_strand` lit les configurations : la file `inotify` continue donc de se vider pendant qu'une configuration de 50 000 services est lue.

```mermaid
flowchart TD
    PHP["php touche X.lck<br/>ou pollers.lck"]
    CFG["set_cache_config_dir()"]
    PEER["add_peer()"]

    subgraph watch["_watch_strand"]
        START["_start_watching()"]
        ARMW["_arm_inotify_wait()<br/>attente sur le descripteur"]
        READ["_read_watch_events()<br/>watch(), route les deux formes"]
        ARMD["_arm_debounce()<br/>500 ms de calme, plafond 5 s"]
        ARMS["_arm_safety_timer()<br/>5 min, ou 5 s si le watch est perdu"]
        FEED["_feed_cache_and_wake_up_resources()<br/>_lck_file_for_poller() ou new-N.prot"]
    end

    subgraph conf["_config_strand"]
        POSTI["_post_individual_work(force_scan)<br/>lot = _lck_set + _scan_for_announcements()"]
        POSTB["_post_batch_work()<br/>lot = _read_poller_batch()"]
        CYCLE["_run_config_cycle(lot)<br/>hash, parse, expand, resolve,<br/>new-X.prot, diff-X.prot"]
    end

    CFG --> START
    CFG -. "si la surveillance tournait déjà :<br/>rattraper ce que le démarrage a manqué" .-> POSTI
    CFG -.-> POSTB
    PEER --> START
    PEER --> FEED
    PHP -. "événement" .-> ARMW

    START --> ARMW
    START --> ARMS
    START -- "force_scan" --> POSTI
    START --> POSTB

    ARMW -- "au réveil" --> READ
    READ -- "se réarmer aussitôt" --> ARMW
    READ -- "des X.lck, ou un rescan à faire" --> ARMD
    READ -- "un pollers.lck :<br/>rien à attendre" --> POSTB
    READ -. "si le watch est perdu :<br/>revenir dans 5 s" .-> ARMS
    FEED -. "rien n'a bougé sur disque" .-> ARMD

    ARMD -- "à l'échéance,<br/>sans scan" --> POSTI
    ARMS -- "à l'échéance" --> READ
    ARMS -- "à l'échéance,<br/>force_scan" --> POSTI
    ARMS -- "à l'échéance" --> POSTB
    POSTI --> CYCLE
    POSTB --> CYCLE
```

Les flèches pleines sont des appels directs. Les pointillées représentent ce qui n'en est pas un : un événement livré par le noyau, un `post()` qui diffère le travail sur un strand, ou un chemin emprunté sous condition.

* **Les deux formes d'annonce suivent des chemins séparés.** `_read_watch_events()` les distingue et n'en fait rien d'autre : un `pollers.lck` va droit à `_post_batch_work()`, des `<X>.lck` passent par le regroupement. Les deux aboutissent au même `_run_config_cycle()`, qui reçoit son lot en paramètre : aucun cycle ne consomme le lot de l'autre chemin, et une annonce de lot n'interrompt pas un regroupement en cours.
* **`_read_watch_events()` réarme l'attente avant tout le reste.** Une rafale plus grosse qu'un tampon laisse le descripteur lisible, l'attente suivante aboutit aussitôt, et la file se vide au lieu de s'étaler sur plusieurs cycles.
* **Le regroupement est la seule voie normale vers le travail pour les annonces individuelles.** `_arm_debounce()` repousse l'échéance à chaque événement. Seul le dernier armement déclenche `_post_individual_work()`, les précédents s'effacent sur `operation_aborted`.
* **La connexion d'un poller doit se réveiller elle-même.** Rien n'a changé sur le disque, donc `inotify` n'a rien à dire : `_feed_cache_and_wake_up_resources()` poste `_arm_debounce()` de sa propre initiative, sans quoi la configuration du poller attendrait le filet de sécurité.

L'état partagé se réduit à trois choses : `_lck_set`, les pollers à traiter, protégé par un mutex puisqu'il est rempli depuis les deux strands ; `_scan_requested_by_watcher`, un atomique écrit côté surveillance et lu côté travail ; et `_burst_started_at`, qui n'appartient qu'à `_watch_strand` et n'a besoin d'aucune protection.

#### Quels événements font foi

Un `<ID>.lck` est vide : seul son nom compte, et n'importe quel événement le concernant suffit. `pollers.lck` a un contenu, et `IN_CREATE` arrive alors que le fichier est encore vide. Seuls deux événements disent qu'un fichier est fini, et `Broker` n'écoute que ceux-là pour le fichier de lot :

* `IN_CLOSE_WRITE`, quand celui qui l'écrivait l'a refermé, ce qui rend l'écriture directe sûre et donc permise par le contrat ;
* `IN_MOVED_TO`, quand il est arrivé par renommage.

Les deux doivent être demandés dans le masque. Sans `IN_MOVED_TO`, le `rename()` que le contrat autorise resterait invisible jusqu'au scan suivant.

Le fichier de lot est lu depuis le worker plutôt qu'au moment de l'événement : un lot qu'`inotify` n'aurait pas signalé est ramassé par le filet de sécurité, qui poste ce chemin lui aussi.

#### Le scan de secours

C'est `inotify` qui détecte les annonces. Le scan du répertoire n'est qu'un filet de sécurité, qui ramasse les `.lck` qu'`inotify` n'a pas signalés. Il tourne dans cinq situations :

1. **au premier cycle**, déclenché dès le démarrage de la surveillance, pour les `.lck` déjà présents, poussés pendant que `Broker` était arrêté ou juste avant que le watcher soit en place ;
2. **quand le répertoire de cache devient connu après ce démarrage**, voir [Démarrage de la surveillance](#démarrage-de-la-surveillance) ;
3. **quand le noyau annonce avoir perdu des événements** (`IN_Q_OVERFLOW`), ce qui arrive quand la file `inotify` sature lors d'un déploiement de masse ;
4. **quand le watch a été perdu puis rétabli**, ce qui s'est passé entre-temps étant invisible ;
5. **à l'échéance du timer de sécurité**, toutes les 5 minutes, pour ce que rien de ce qui précède ne couvre, comme un système de fichiers où `inotify` ne fonctionne pas. Ce timer relit aussi les événements, et c'est le seul chemin qui rappelle `watch()`, donc le seul qui puisse rétablir un watch dont la reprise avait échoué.

Tout ce que le scan retrouve est traité : une annonce encore présente est, par construction, une poussée que `Broker` n'a pas lue, puisqu'il ne la laisse jamais survivre au cycle qui la lit.

⚠️ Un débordement de la file ou un watch rétabli ne nomme aucun poller. Il n'appartient donc à aucun des deux chemins et est rapporté à part (`watch_report::rescan_requested`). Sans cela, une lecture qui ne rapporte que ça ne posterait aucun travail, et le scan attendrait le timer de sécurité.

#### Perte du watch `inotify`

Un watch `inotify` porte sur un inode, pas sur un chemin. Si le répertoire de cache est supprimé, renommé ou remplacé, le noyau livre `IN_IGNORED`, `IN_DELETE_SELF` ou `IN_MOVE_SELF`, puis plus rien n'est signalé. `directory_watcher` traite ces trois événements en rétablissant le watch sur son chemin d'origine et en demandant un scan. Trois précautions :

* `IN_DELETE_SELF` et `IN_MOVE_SELF` doivent être demandés dans le masque, contrairement à `IN_IGNORED` et `IN_Q_OVERFLOW` que le noyau livre de lui-même. Sans eux, un répertoire renommé cesserait silencieusement d'être surveillé, le watch restant vivant sur l'inode déplacé.
* L'ancien watch est retiré avant l'ajout du nouveau. Un répertoire renommé garde un watch actif sur l'inode déplacé : ne pas le retirer fuirait un watch à chaque renommage.
* Si le rétablissement échoue, parce que le répertoire n'existe pas encore, il est retenté 5 s plus tard, et l'échec n'est journalisé qu'une fois.

#### Démarrage de la surveillance

La surveillance a deux déclencheurs, et c'est le premier arrivé qui la met en route : `set_cache_config_dir()`, quand la configuration de `Broker` est appliquée, et `add_peer()`, quand un poller se connecte. L'ordre n'est pas garanti : un poller déjà lancé se connecte parfois quelques dizaines de millisecondes avant que la configuration soit appliquée.

Quand c'est `add_peer()` qui gagne, la surveillance démarre sans connaître le répertoire de cache :

1. l'attente `inotify` ne peut pas être armée, faute de descripteur ;
2. le premier cycle ne scanne rien, `_cache_config_dir` étant vide ;
3. `_lck_file_for_poller()` renvoie 0 pour le poller qui vient de se connecter, dont le `.lck` n'est donc pas cherché.

Quand `set_cache_config_dir()` trouve la surveillance déjà lancée, elle doit donc armer l'attente et redemander un cycle avec scan. Armer ne suffit pas : `inotify` ne signale que ce qui arrive après l'armement, et le poller qui a déclenché ce démarrage précoce est justement celui dont la configuration a été manquée. L'armement est idempotent (`_inotify_wait_armed`) : deux attentes sur un même descripteur se déclencheraient toutes les deux pour un seul événement.

Dans `apply()`, le répertoire des configurations de pollers est renseigné avant `set_cache_config_dir()`, jamais après. Poser le répertoire de cache démarre la surveillance, dont le cycle écrit `new-<ID>.prot` : sans répertoire de destination, ce chemin serait relatif au répertoire de travail de `cbd`.

## Split de broker::config::applier::state

Cet objet `broker::config::applier::state` est utilisé à la fois par
`cbmod` et par `broker`. Il gère la surveillance des fichiers de
configuration `Engine`, le calcul des différences, etc...

Son comportement est un peu différent selon qu'il est utilisé par
`cbmod` ou par `broker`.

Garder un unique objet pour les deux nous oblige à garder des attributs
utilisés par l'un et pas par l'autre. La liste des peers, par exemple,
a une gestion assez compliquée pour gérer les deux cas. L'attribut `_engine_conf`,
utilisé uniquement côté `cbmod` se retrouve aussi dans `broker` alors qu'il ne
sert à rien.

La classe `com::centreon::broker::config::applier::state` est donc découpée en trois :

```mermaid
classDiagram
    class state {
        -PeerType _peer_type;
        -string _cache_dir;
        -uint32_t _poller_id;
        -uint32_t _rpc_port;
        -bbdo_version _bbdo_version;
        -string _poller_name;
        -string _broker_name;
        -size_t _pool_size;
        -broker_cache _global_cache
        -shared_ptr<spdlog::logger> _logger;

        +add_peer()
        +_run_config_cycle()
        +_prepare_diff_for_poller()
        etc...
    }
    class cbmod_state {
        -cbmod_state& _state
        -path _proto_conf;
        -unique_ptr<DiffState> _diff_state
        -atomic_bool _diff_state_applied
        +specific_negotiate(Welcome& obj)
        +write(const std::shared_ptr<io::data>& d)
        +stop()
    }
    class peer_registry {
        -shared_ptr<logger> _logger
        -Mutex _connected_peers_m
        -flat_hash_map<uint64_t, engine_peer> _engine_peers
        -flat_hash_map<peer_key, broker_peer> _broker_peers
        -flat_hash_map<peer_key, unknown_peer> _unknown_peers
        -flat_hash_map<uint64_t, uint64_t> _poller_to_relay
        -_peer_needs_update(const engine_peer& peer)$static$
        +add_peer(...) string
        +remove_peer(...) bool
        +register_poller_via_relay(...) uint64_t
        +connected_pollers()
        +connected_peers()
        +is_poller_connected(uint64_t poller_id)
        +is_poller_running(uint64_t poller_id)
        +set_instance_running(uint64_t poller_id, bool running)
        +poller_timezone(uint64_t poller_id)
        +broker_knows_poller_conf(uint64_t poller_id)
        +set_broker_knows_poller_conf(uint64_t poller_id, bool known)
        +poller_needs_update(uint64_t poller_id)
        +set_poller_conf_sent(uint64_t poller_id, string_view version) bool
        +acknowledge_poller_conf(uint64_t poller_id, const string& version) bool
        +leave_conf_round(const vector<uint64_t>& poller_ids)
        +try_close_conf_round()
        +lock_engine_peer(uint64_t poller_id) locked_engine_peer
        +topology_cache() TopologyCache
        +restore_pollers_from_cache(const TopologyCache& cache)
    }
    class broker_state {
        -path _cache_config_dir
        -path _pollers_config_dir
        -unique_ptr<directory_watcher> _cache_config_dir_watcher
        -peer_registry _peers
        -flat_hash_map<uint64_t, string> _engine_configuration
        -flat_hash_map<uint64_t, pair<string,string>> _pending_config_requests
        -flat_hash_map<uint64_t, vector<uint64_t>> _pending_config_revokes
        -unique_ptr<steady_timer> _safety_timer
        -unique_ptr<steady_timer> _debounce_timer
        -flat_hash_set<uint32_t> _lck_set
        -Mutex _conf_handover_m
        -_prepare_diff_for_poller(uint64_t poller_id, unique_ptr<State>&& state)
        -_integrate_acknowledged_diffs()
        -_publish_global_diff(const vector<uint64_t>* poller_ids)
        -_start_watching()
        -_lck_file_for_poller(uint32_t poller_id)
        -_read_watch_events() watch_report
        -_post_individual_work(bool force_scan)
        -_post_batch_work()
        -_run_config_cycle(const flat_hash_set<uint32_t>& pollers_set)
        +add_peer()
        +remove_peer()
        +is_poller_connected(uint64_t poller_id)
        +is_poller_running(uint64_t poller_id)
        +set_instance_running(uint64_t poller_id, bool running)
        +connected_peers()
        +connected_pollers()
        +poller_needs_update(uint64_t poller_id)
        +set_poller_conf_sent(uint32_t poller_id, string_view version) bool
        +acknowledge_poller_conf(uint64_t poller_id, const string& version) bool
        +on_poller_conf_acknowledged(uint64_t poller_id, const string& version)
        +set_poller_engine_conf()
        +apply(state& s, bool run_mux = true)
    }
    state <|-- cbmod_state
    state <|-- broker_state
    broker_state *-- peer_registry
```

`broker_state` ne possède plus aucune map de pairs : elles vivent toutes dans
`peer_registry`, avec leur propre mutex. Ce qui reste sous
`broker_state::_connected_peers_m`, ce sont uniquement les files de transfert —
`_pending_config_requests`, `_pending_config_revokes`, `_pending_diff_states`,
`_pending_diff_state_acks` et `_engine_configuration`.

Le découpage de `state` implique aussi le découpage de la classe `bbdo::stream`.

Il y a les `bbdo::stream` utilisés par `cbmod` pour se connecter à un `broker`,
il y a les `bbdo::stream` utilisés par `broker` pour se connecter à un `engine`
et il y a aussi les `bbdo::stream` utilisés par des fichiers cache ou *unprocessed*.

pour représenter tout ça, il a fallu découper `bbdo::stream` en une classe de base `bbdo::basic_stream` puis une
classe virtuelle pure `bbdo::stream` qui hérite de `bbdo::basic_stream` et enfin deux classes concrètes
`bbdo::broker_stream` et `bbdo::cbmod_stream` qui héritent de `bbdo::stream`.

Ce qui donne la structure suivante :

```mermaid
classDiagram
    class basic_stream {
        +write()
    }
    class stream {
        -bool _extended_negotiation
        -bool _negotiate
        -bool _negotiated
        -std::list<std::shared_ptr<io::extension>> _extensions
        -std::string _get_extension_names(bool mandatory) const
        +enum negotiation_type
        +negotiate()
        +set_negotiate(bool negotiate)
    }
    class broker_stream {
        -broker_state& _state
        -_handle_bbdo_event(const std::shared_ptr<io::data>& d)
        +specific_negotiate(Welcome& obj)
        +bool read(std::shared_ptr<io::data>& d, time_t deadline)
        +int32_t stop()
    }
    class cbmod_stream {
        -cbmod_state& _state
        +specific_negotiate(Welcome& obj)
        +int32_t write(const std::shared_ptr<io::data>& d)
        +int32_t stop()
    }
    basic_stream <|-- stream
    stream <|-- broker_stream
    stream <|-- cbmod_stream
```

### Cycle de vie de broker_state, du cache et du downtime_manager

`broker_state` orchestre deux objets à longue durée de vie dont l'ordre de
création et de destruction est important :

* le **cache global** (`broker_cache`, possédé par la classe de base `state` via
  `_global_cache`), persisté dans `<cache_dir>.cache` et rechargé au démarrage
  suivant ;
* le **gestionnaire de downtimes**
  (`com::centreon::common::downtimes::downtime_manager`, un singleton), chargé
  **uniquement** lorsque `notification_mode=broker`. Il possède les downtimes
  in-process (planifiés via gRPC et downtimes hérités de BAM).

Quand Broker gère les downtimes, les downtimes *démarrés* (en cours) doivent
survivre à un redémarrage de `cbd` : ils sont sauvegardés avec le cache à l'arrêt
et réinjectés dans le gestionnaire au démarrage suivant. Les downtimes non
démarrés peuvent être perdus ; le `scheduled_downtime_depth` est recalculé de
façon idempotente (un comptage par ressource), il n'est pas incrémenté
aveuglément.

#### Démarrage

La subtilité clé : le cache est créé **à l'intérieur** de `state::apply()`
(`initialize_cache()`), et `_global_cache` n'est affecté qu'une fois le
constructeur de `broker_cache` terminé. Le chemin de réinjection rappelle
`config::applier::state::instance().cache()` (via le `resource_exists()` des
callbacks de downtime), il ne **doit donc pas** s'exécuter depuis le constructeur
— sinon il déréférencerait un `_global_cache` non encore affecté et provoquerait
un `abort`. Le constructeur se contente donc de charger les downtimes persistés
dans une liste en attente, et la réinjection a lieu plus tard, une fois
`_global_cache` en place.

```mermaid
sequenceDiagram
    participant main as cbd main
    participant BS as broker_state
    participant S as state (base)
    participant CACHE as broker_cache
    participant DM as downtime_manager
    participant ENG as Engine (centralisé)

    main->>BS: apply(config s, run_mux)
    Note over BS: lecture params["notification_mode"]
    alt notification_mode == broker
        BS->>DM: load(broker_downtime_callbacks(io_context))
        BS-->>main: log CORE "downtime management enabled,<br/>downtime manager loaded"
    end

    BS->>S: state::apply(s, run_mux)
    S->>S: _modules.apply(...)
    S->>CACHE: initialize_cache() → make_unique<broker_cache>()
    activate CACHE
    CACHE->>CACHE: _load_cache() lit <cache_dir>.cache
    Note over CACHE: remplit seulement _pending_active_downtimes<br/>PAS de réinjection ici :<br/>_global_cache pas encore affecté
    deactivate CACHE
    S->>S: endpoint::apply() puis arme la barrière de readiness<br/>(le démarrage du moteur est différé, voir plus bas)
    S-->>BS: retour (_global_cache désormais affecté)

    alt notification_mode == broker (depuis _on_barrier_released(), une fois la barrière levée)
        BS->>CACHE: reinject_pending()
        Note over CACHE: mode legacy : ressources connues → réinjection immédiate<br/>mode centralisé : ressources inconnues → no-op (différé)
    end

    Note over ENG,CACHE: mode centralisé : les ressources arrivent plus tard
    ENG->>CACHE: pb_engine_state → merge(State)
    CACHE->>CACHE: reinject_pending()
    loop chaque downtime en attente dont le host/service existe désormais
        CACHE->>DM: reload_started_downtime(...) (conserve l'id sauvegardé)
        DM->>DM: downtime::reload() (passe in-effect, planifie seulement le check de fin)
    end
    CACHE->>CACHE: recalcule depth = comptage par (host,service), pose dans le cache
    CACHE-->>ENG: publie pb_adaptive_{host,service}_status (depth restaurée)
```

#### Barrière de readiness au démarrage

Le moteur de multiplexing n'est **pas** démarré à la fin de `state::apply()` dès
que les endpoints sont créés. Le faire entre en course avec le démarrage des
streams de sortie : un output (par ex. le `monitoring_stream` BAM) s'ouvre sur son
thread de `failover` et publie ses *définitions* de démarrage (comme le service
virtuel de la BA, avec `scheduled_downtime_depth=0`) tandis que, en parallèle, le
moteur est démarré et l'état persisté réinjecté. Une définition périmée traitée
*après* la profondeur de downtime hérité réinjectée l'écraserait (profondeur de la
BA de retour à 0).

Pour rendre l'ordre déterministe, la base `state` installe une **barrière de
readiness au démarrage**. Elle vit volontairement dans la classe de base, afin de
protéger à la fois `cbd` et `cbmod` — par ex. un output Lua chargé sur `cbmod` qui
restaure un état est protégé de la même manière :

* à la fin d'`apply()`, au lieu de démarrer le moteur, `_enable_multiplexing()`
  recense l'ensemble des endpoints de **sortie** qui viennent d'être créés et arme
  la barrière (en mode `--check`, `run_mux=false`, elle ne fait rien) ;
* chaque stream de sortie, une fois que son `failover` a terminé sa **première**
  tentative d'`open()` — succès *ou* échec, pour qu'une base/un pair down ne bloque
  pas le démarrage — appelle `state::notify_output_ready(name)` exactement une fois
  (one-shot par failover) ;
* quand tous les outputs attendus se sont enregistrés, `_maybe_release_barrier()`
  démarre le moteur de multiplexing **une seule fois**, ce qui flushe les
  événements bufferisés tant qu'il était `not_started` (les définitions de
  démarrage), puis invoque le hook `_on_barrier_released()` ;
* un **timeout** (60 s) lève la barrière de toute façon, pour que Broker/Engine ne
  reste jamais muet si un output ne s'ouvre jamais.

Comme le moteur bufferise tout tant qu'il est `not_started`, les définitions de
démarrage sont flushées en premier ; la réinjection — publiée soit réactivement
quand la configuration Engine arrive (mode centralisé), soit depuis
`_on_barrier_released()` (mode broker legacy) — est donc toujours ordonnée
**après** elles, et la `scheduled_downtime_depth` restaurée l'emporte.

`_on_barrier_released()` est la seule partie de la barrière spécifique à une
sous-classe : `broker_state` la surcharge pour appeler
`reinject_pending()` ; la base (et `cbmod_state`) la laissent vide.

```mermaid
sequenceDiagram
    participant S as state::apply (base)
    participant EP as endpoint::apply
    participant FO as failover (par output, thread worker)
    participant B as barrière (dans la base state)
    participant ENG as moteur de multiplexing

    S->>EP: crée les streams de sortie (les threads failover démarrent)
    S->>B: _enable_multiplexing() : arme avec expected = noms des endpoints de sortie
    par chaque output, sur son propre thread failover
        FO->>FO: open() (charge DB/cache ou se connecte) + publie les définitions de démarrage
        FO->>B: notify_output_ready(name) (one-shot, après la 1ʳᵉ tentative d'open)
    end
    Note over B: tous les attendus prêts, ou timeout 60 s
    B->>ENG: start() (flushe les définitions de démarrage bufferisées)
    B->>S: _on_barrier_released() (broker_state → reinject_pending)
```

#### Arrêt

`broker_state` est détruit via `state::unload()` (`delete gl_state`). Le
destructeur dérivé `~broker_state` s'exécute **avant** la base `~state`, donc le
cache est encore vivant quand `~broker_state` lui transmet les downtimes
démarrés. Le cache est ensuite persisté par `~broker_cache` (appelé depuis
`~state`), dans les **deux** modes de configuration — seules les sections lourdes
(hosts, services, groupes…) restent réservées au mode legacy ; les downtimes
actifs sont toujours sauvegardés.

```mermaid
sequenceDiagram
    participant main as cbd main
    participant BS as broker_state (~broker_state)
    participant DM as downtime_manager
    participant CACHE as broker_cache
    participant S as state (~state, base)

    main->>BS: state::unload() → delete gl_state
    BS->>BS: annule _watch_engine_conf_timer
    BS->>BS: save_topology_cache()
    alt downtime_manager::is_loaded()
        BS->>DM: get_scheduled_downtimes()
        DM-->>BS: tous les downtimes planifiés
        Note over BS: garde seulement is_in_effect()<br/>construit les Downtime protos (id, depth, dates…)
        BS->>CACHE: set_active_downtimes(active)
    end
    BS->>DM: unload() (détruit le gestionnaire + callbacks)
    Note over BS: corps de ~broker_state terminé

    BS->>S: ~state (destructeur de base)
    S->>CACHE: destruction de _global_cache → ~broker_cache
    activate CACHE
    CACHE->>CACHE: _save_cache() → écrit <cache_dir>.cache
    Note over CACHE: sections 8-14 persistées dans les deux modes<br/>sections lourdes seulement en mode legacy<br/>(voir « Ce qui est persisté à l'arrêt »)
    deactivate CACHE
```
