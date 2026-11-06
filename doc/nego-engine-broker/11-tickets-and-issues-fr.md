# Tickets

[← Précédent](10-poller-ha-fr.md) · [Index](README-fr.md)

<!-- TOC -->
* [Premiers tickets](#premiers-tickets)
  * [Check health interne à Engine avec remontée à Broker](#check-health-interne-à-engine-avec-remontée-à-broker)
  * [A propos du calcul de diff](#a-propos-du-calcul-de-diff)
  * [Étude sur le mécanisme de répartition](#étude-sur-le-mécanisme-de-répartition)
  * [Introduction des messages Zone et DiffZone](#introduction-des-messages-zone-et-diffzone)
* [Plusieurs tickets en parallèle](#plusieurs-tickets-en-parallèle)
  * [Préparation d'unified\_sql](#préparation-dunified_sql)
  * [Cache centralisé](#cache-centralisé)
  * [Le bloc de conversion](#le-bloc-de-conversion)
* [Évolutions](#évolutions)
  * [Amélioration du bloc de conversion](#amélioration-du-bloc-de-conversion)
  * [Récupération du check Health](#récupération-du-check-health)
* [Déplacement de l'envoi des commandes externes sur Broker](#déplacement-de-lenvoi-des-commandes-externes-sur-broker)
<!-- TOC -->

## Premiers tickets

### Check health interne à Engine avec remontée à Broker

Engine récupère ses indicateurs de santé pour envoyer un message Health à Broker:
```protobuf
message Health {
  uint32 poller_id    = 1;
  float  latency_avg  = 2;  // moyenne glissante de (actual_start − scheduled_start) en secondes
  uint32 queue_depth  = 3;  // nombre d'événements en attente dans la file de l'event loop
}
```

Ce format de message donne un but, mais n'est pas à prendre comme définitif. Une étude est nécessaire sur les infos utiles pour la répartition.

Un système classique est de pondérer les différents indicateurs afin de calculer une charge globale.
Pour arriver à ça, c'est bien que tous les paramètres soient entre 0 et 1.
Pour le cpu et la mémoire, le message contient déjà l'information à la bonne échelle.
Pour la latence, il faut définir un seuil au-delà duquel on considère que le poller est surchargé. Par exemple, si la latence est supérieure à 10s, on considère que le poller est surchargé.

On peut maintenant calculer la charge globale du poller avec le formule suivante :
C=alpha * cpu + beta * mem + gamma * latency / latency\_max avec alpha + beta + gamma = 1.

Si on trouve un autre paramètre intéressant, on peut toujours l'ajouter à la formule et rééquilibrer les poids.

Une étude plus poussée pourrait nous permettre de déterminer les poids optimaux pour chaque poller. Mais de manière empirique,
on peut commencer avec alpha = 0.4, beta = 0.2 et gamma = 0.4. Il semble que le cpu et la latence aient un impact plus important
que la mémoire d'où ces paramètres.

Une fois qu'on a cette charge, on peut définit un intervalle de charge limite. Par exemple,
de 60% à 80% ; l'intérêt d'avoir un intervalle est de pouvoir ainsi travailler avec un hysteresis.

* Lorsqu'on veut augmenter la charge d'un poller, on s'autorise à le faire tant que la charge est inférieure à 60%.
* on considère un poller trop chargé quand il dépasse 80%.
* Lorsqu'il est trop charge (charge > 80%), on essaie de réduire sa charge pour qu'il repasse sous les 60%.

L'intervalle [60% ; 80%] doit être configurable.

Lorsqu'un poller est trop chargé (donc charge > 80%), on doit lui retirer des hosts pour qu'il repasse sous les 60%.
Le 60% est un objectif, il est difficile de savoir exactement le nombre de hosts à retirer pour l'atteindre. Le tout
est donc de finir dans l'intervalle.

On a donc quelques paramètres de configuration à définir:
* latence maximale (par exemple 10 secondes)
* poids de la charge CPU, mémoire et latence (configurable)
* intervalle de charge entre 60% et 80% (configurable)

### A propos du calcul de diff

Broker reçoit les conf des zones, ce qui donne new-zone-1.prot, new-zone-2.prot, ...
Ces zones tournent déjà et leur conf est dans zone-1.prot, zone-2.prot, ...

On calcule ensuite diff-zone-1.prot, diff-zone-2.prot, ...

Si zone 1 contient les pollers 1, 2 et 3.

Le but serait de répartir les modifications sur chaque poller en fonction de sa charge. Et on utiliserait
la même machinerie que lors de la surveillance des pollers.

### Étude sur le mécanisme de répartition
Quelle charge remonter à broker et comment Broker doit adapter la répartition à ces charges ?
C'est surtout la seconde partie de la question qui nous intéresse.

### Introduction des messages Zone et DiffZone
le calcul de configuration initial par broker n'est plus du State/DiffState mais du Zone / DiffZone.

La Zone est un message très similaire au State, elle contient la configuration des
pollers de la zone, pour le moment on considère que tous les pollers sont configurés
de la même manière. Et peut-être que cela évoluera avec le temps.
La zone porte tous les hosts/services devant être gérés par les pollers
qu'elle contient. Par contre, contrairement au State, la Zone contient une liste
de pollers. Si les paramètres globaux définis au niveau de la Zone doivent être
différenciés par poller, on peut les déplacer de la zone vers les pollers (au
niveau des définitions de message).

Le contenu de centengine.cfg est une première ébauche de la configuration pour la zone. Mais on doit ajouter
un tableau par poller puisqu'il y a déjà des hosts spécifiques à chacun.
On peut aussi imaginer des niveaux de logs différents par poller.
Par contre, la durée de check minimale doit être partagée sur toute la zone.

Il reste encore à déterminer quels champs déplacer par poller.

13 points.

## Plusieurs tickets en parallèle

### Préparation d'unified\_sql
Le but est d'écrire la conf préparée par broker d'une traite dans centreon\_storage.

21 points.

### Cache centralisé
Tous les streams doivent accéder au même cache qui provient de la configuration en grande partie.

Il est composé de deux parties :
* configuration
* temps réel et autre (l'héritage des caches broker actuels)

Il faut recenser les caches actuellement utilisés. Et ensuite on produit un cache global pour alimenter l'ensemble.

On a la configuration engine globale qui est enrichie au fur et à mesure des envois de conf côté broker.
Le cache est à part et pointe sur cette configuration générale.
En faisant ça, côté Engine, le cbmod a aussi son cache qui pointe ce coup ci vers la conf Engine dans globals.cc.
L'alimentation est templatisée car faite soit par Zone, soit par State.

Possibilité de découper :
1. on fait le nouveau cache. Encore assez lourd mais raisonnable.
2. migrer progressivement les autres caches vers celui-là. Les migrations peuvent être faites en parallèle (influxdb, graphite, VictoriaMetrics, Lua, rrd, unified\_sql).
3. Un ticket existe déjà pour supprimer les outputs sql et storage.

### Le bloc de conversion
DiffZone vers DiffState, à partir de la zone, on lance un DiffState par poller.

Une première étape est de considérer tous les pollers identiques, et un round-robin fait l'affaire.

## Évolutions
### Amélioration du bloc de conversion
En partant de l'étude, la répartition sur les pollers doit pouvoir être améliorée.
Le bloc doit pouvoir être paramétrable à l'aide du fichier de conf de broker. Un algorithme ne suffira sûrement pas à gérer tous les cas.
### Récupération du check Health
La prise en compte par Broker du Health doit permettre le rééquilibrage des configurations des pollers.

# Soucis potentiels à résoudre
* commandes externes
* services passifs problématiques
* agent
* retention.dat (côté poller, si ça change on n'a plus l'info) : design arrêté, voir
[Prérequis 5 : élimination de `retention.dat`](09-ha-preparatory-work-fr.md#prérequis-5--élimination-de-retentiondat).
* hostdependencies / servicedependencies, dépendances d'**exécution** : évaluées localement
par le poller (`host::authorized_by_dependencies`, `service::authorized_by_dependencies`) et
elles inhibent le check lui-même, décision synchrone dans le chemin chaud du scheduler — la
co-localisation reste donc obligatoire. Validation : traitée — une dépendance inter-pollers
est une erreur de configuration détectée au `resolve`, grâce à la vue globale que Broker a
des configurations des autres pollers (`foreign_objects`). Attention : un maître absent rend
`master_*_ptr` nul et la dépendance est alors *silencieusement ignorée*, d'où la validation.
* dépendances de **notification** : déjà évaluées par Broker (`broker_cache`), elles
pourraient franchir les pollers. Refusées aujourd'hui malgré tout, faute de domicile pour une
dépendance dont aucun des deux pollers ne connaît l'autre extrémité — elles contraignent donc
encore le placement, et les blocs de co-localisation se calculent sur les deux types.
* Hostescalation / Serviceescalation : les escalades doivent suivre l'objet notifié (contrainte de co-location déjà appliquée par l'algorithme de distribution ; gérée par le service de notification en mode configuration centralisée).
* Downtimes et acquittements : traités — voir [Downtimes et acquittements centralisés](10-poller-ha-fr.md#downtimes-et-acquittements-centralisés) et [Notifications en mode HA](10-poller-ha-fr.md#notifications-en-mode-ha).
* Anomalydetection doit être sur le même poller que le service associé. Et sa conf doit suivre.
* Très difficile de garder la compatibilité avec l'ancien comportement d'engine
* ping-pong
* le check de la configuration Engine en gRPC sur Broker : traité — RPC `CheckPollerConfig`. Reste l'extension multi-pollers (sections par poller et section globale).
* dans la table resources, nous n'avons actuellement que poller_id, est-il judicieux d'aussi ajouter la zone_id ? Première
impression : oui. Même si globalement nous remplaçons poller_id par zone_id, il y a des exceptions !! Les pollers ID gardent
leur sens par exemple pour accéder aux logs.

# Résolution des soucis
## Déplacement de l'envoi des commandes externes sur Broker
On crée les points d'entrée pour toutes les commandes externes Engine sur Broker.
Et Broker, en interne, envoie la demande au poller concerné.

Le point d'entrée est la RPC générique `ExecuteExternalCommand(ExternalCommandRequest
{ command })` de `brokerrpc` : elle reçoit la ligne legacy telle que PHP la produit
déjà pour le tube de commandes. Broker n'en lit que le nom de la commande et le nom
d'hôte (premier argument), résout le poller propriétaire dans son cache et pousse la
ligne complète, préfixée de l'horodatage, dans l'événement BBDO `pb_external_command`
sur le canal descendant de ce poller (le même que `pb_notification_execute`, vidé par
le `read()` du flux du poller, soit moins de 100 ms). Côté poller, `cbmod` la remet au
parseur du tube (`commands::processing::execute`) : les commandes thread-safe
(`PROCESS_*_CHECK_RESULT`) sont exécutées immédiatement sur le thread de réception,
comme le fait le lecteur du tube, les autres attendent l'itération suivante de la
boucle d'événements. Aucun des handlers de commande d'Engine n'est modifié.

La table de routage (`common/external_commands`) classe chaque commande par cible :
processus, globale, hôte, service, hostgroup, servicegroup, contact, contactgroup,
downtime, commentaire. Un test unitaire d'Engine vérifie qu'elle liste exactement les
commandes du parseur. Hôte et service vont au poller propriétaire de l'hôte, avec les
`host_id` / `service_id` résolus dans le cache : Engine réécrit les noms à partir des
identifiants avant de parser (`processing::resolve_ids`), la ligne peut donc porter
des noms périmés ou vides. Contact et contactgroup sont diffusés à tous les pollers
connectés (chacun a sa copie). Globale et processus vont au poller nommé dans la
requête (`PollerIdentifier`, obligatoire : pas de diffusion implicite d'un
`RESTART_PROGRAM`). Les groupes ne sont plus émis par PHP et les downtimes /
commentaires désignés par identifiant restent `UNIMPLEMENTED`. En
`notification_mode = broker`, les commandes que Broker possède
(acquittements, downtimes, commentaires, bascules de notification) sont **exécutées par
Broker lui-même** : `legacy_commands` convertit les arguments positionnels en requête
typée et la méthode RPC existante est appelée comme une méthode ordinaire, aucune logique
métier dupliquée (`broker_impl::_execute_native_command`). Routées au poller, elles
seraient silencieusement ignorées par Broker. Seules les suppressions de downtime par
critères, sans RPC typée, restent `UNIMPLEMENTED`.

La couverture de test du routage est gratuite grâce à l'outillage Robot : toute ligne
legacy émise par les mots-clés d'`Engine.py` (`Ctn Process Service Check Result`,
`Ctn Acknowledge Service Problem`, `Ctn Disable Host Check`, ...) passe par un helper
unique, `Ctn Send External Command`, qui l'écrit dans le tube de commandes du poller par
défaut et la confie à `ExecuteExternalCommand` quand la variable Robot `${EXTCMD_MODE}`
vaut `broker` (`Ctn Set External Command Mode    broker` dans un test, ou
`--variable EXTCMD_MODE:broker` pour toute une campagne). Les tests BBDO3
d'`external-commands.robot`, `external-commands2.robot` et `acknowledgement.robot`
tournent en mode broker, leurs jumeaux BBDO2 gardent le tube : les deux voies restent
couvertes sans doubler la campagne.

> Pour le passage des commandes externes à gRPC côté PHP et la règle de routage
> Engine/Broker selon `notification_mode`, voir
> [Évolutions PHP — Commandes externes via gRPC](../php-evolutions-fr.md#évolution-2--commandes-externes-via-grpc).
