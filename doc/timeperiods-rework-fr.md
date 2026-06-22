# Timeperiods — extraction en bibliothèque partagée et choix d'implémentation

## Sommaire

  - [1. Contexte et objectif](#1-contexte-et-objectif)
  - [2. Le concept de timeperiod](#2-le-concept-de-timeperiod)
  - [3. État du code actuel](#3-état-du-code-actuel)
  - [4. Points de couplage à engine](#4-points-de-couplage-à-engine)
  - [5. Comparatif des options d'implémentation](#5-comparatif-des-options-dimplémentation)
  - [6. Synthèse par option](#6-synthèse-par-option)
  - [7. Recommandation](#7-recommandation)
  - [8. Plan d'extraction](#8-plan-dextraction)
  - [9. Pistes à explorer](#9-pistes-à-explorer)

Ce document analyse l'extraction du code des *timeperiods* d'engine vers une
bibliothèque partagée réutilisable par broker (notamment pour l'évaluation des
plages de notification côté Broker, cf. branche `MON-187019-broker-notification`),
et compare trois options d'implémentation si on devait les refaire de zéro :
le code actuel, Boost.Date_Time et Abseil.

## 1. Contexte et objectif

Le calcul des timeperiods (plages horaires de monitoring et de notification) est
aujourd'hui confiné dans engine. On veut le rendre **indépendant d'engine** pour
pouvoir l'utiliser aussi dans broker.

Le travail a démarré par un déplacement de fichiers vers `engine/src/timeperiods/`
avec un `CMakeLists.txt` produisant une lib statique `timeperiods` :

```
engine/src/timeperiods/
  ├── CMakeLists.txt        # add_library(timeperiods STATIC ...)
  ├── timeperiod.{cc,hh}
  ├── daterange.{cc,hh}
  ├── timerange.{cc,hh}
  └── timeperiod_types.hh
```

Les en-têtes ont été déplacés depuis `engine/inc/com/centreon/engine/` ;
les includes consommateurs utilisent désormais le chemin
`engine/src/timeperiods/...`.

## 2. Le concept de timeperiod

**Point essentiel : une timeperiod n'est pas un intervalle, c'est un moteur de
récurrence.** Elle décrit un motif qui se répète :

- plages par jour de semaine (`monday 09:00-17:00`) ;
- calendar dates, month dates, **month-week-day** (« le 3ᵉ lundi de chaque
  mois ») ;
- exceptions (dateranges) avec **skip interval** (« tous les N jours ») ;
- **exclusions** (une timeperiod en exclut une autre) ;
- gestion DST (heure d'été/hiver) ;
- et surtout l'opération clé `get_next_valid_time` / `check_time_against_period`
  (« à partir de cet instant, quel est le prochain moment couvert »).

L'analogue conceptuel est la `RRULE` iCalendar, **pas** un type intervalle de lib
de dates. C'est fondamental pour la suite : **aucune** bibliothèque de dates ne
fournit ce concept. Quel que soit le choix, le moteur de récurrence reste du code
maison ; ce qui change, c'est la qualité des primitives calendaires/fuseau
sous-jacentes.

## 3. État du code actuel

- ~1600 LOC au total (`timeperiod.cc` ~1219, `daterange.cc` ~314,
  `timerange.cc` ~79), héritage Nagios.
- Implémentation entièrement bâtie sur `struct tm` + `mktime` + `localtime_r`,
  avec arithmétique calendaire à la main (année/mois/jour de semaine).
- DST géré manuellement par le motif `tm_isdst = -1` puis re-normalisation après
  `mktime` (cf. commentaires « There was a DST shift in between »). Les tests
  `engine/tests/timeperiod/get_next_valid_time/dst_forward.cc` et
  `dst_backward.cc` existent précisément parce que ce point est délicat.
- **Le fuseau horaire est géré via un état global de process** :
  `engine/src/timezone_manager.cc` fait `setenv("TZ", …)` puis `tzset()` autour
  des calculs. C'est l'approche héritée de Nagios — un side-effect global,
  **thread-unsafe**.

## 4. Points de couplage à engine

Pour que la lib soit réellement réutilisable dans broker, il faut casser :

1. **Le registre global statique** `static timeperiod_map timeperiods;` porté par
   la classe. Broker ne peut pas partager la map globale d'engine ; la résolution
   des exclusions (`resolve()` → `timeperiod::timeperiods.find(...)`) doit
   recevoir la collection en paramètre plutôt que la lire en statique.
2. **`config_logger`** (via `globals.hh`) — logger global d'engine ; à remplacer
   par un `std::shared_ptr<spdlog::logger>` injecté.
3. **`engine_error`** (via `exceptions/error.hh`) — à remplacer par
   `com::centreon::exceptions::msg_fmt` (déjà dans `common/`).
4. **`contains_illegal_object_chars`** (via `shared.hh`) — petit utilitaire à
   descendre dans la lib ou dans `common/`.
5. **`daterange` / `timerange`** font partie du même cluster et doivent perdre
   leur dépendance à `engine/common.hh` (on n'a besoin que de `DATERANGE_TYPES`,
   des `time_t` et de quelques constantes).
6. Includes morts à supprimer : `broker.hh` et le
   `using namespace ...::applier;` dans `timeperiod.cc` (aucun symbole utilisé).

À terme, la lib devrait vivre sous `common/` (et non `engine/src/`) pour éviter
une dépendance inversée broker → engine, et changer de namespace
(`com::centreon::common::timeperiods` plutôt que `com::centreon::engine`).

## 5. Comparatif des options d'implémentation

Rappel : les trois options imposent d'écrire soi-même le moteur de récurrence.
La différence porte sur les primitives calendaires et de fuseau.

| Critère | Code actuel (`struct tm`/`mktime`) | Boost.Date_Time | Abseil `absl/time` |
|---|---|---|---|
| Concept récurrence fourni | Oui (c'est *ton* moteur) | Non | Non |
| Primitives calendaires | `struct tm` brut, calcul manuel | Riche : `nth_kday_of_month`, date iterators | `CivilDay`, `GetWeekday`, `NextWeekday`/`PrevWeekday` |
| « 3ᵉ lundi du mois » | ~30 lignes manuelles | **1 appel** (`nth_day_of_the_week_in_month`) | ~5 lignes composées |
| Correction DST | Manuelle, fragile (`tm_isdst=-1`) | Correcte via `local_time` | **Excellente** : `TimeInfo` expose instants sautés/ambigus |
| Fuseau par-timeperiod | ❌ global process (`setenv TZ`+`tzset`) | ✅ `time_zone_ptr` par objet | ✅ `absl::TimeZone` par objet (tzdata IANA) |
| Thread-safety fuseau | ❌ mutation globale | ✅ | ✅ (value type immuable) |
| Déjà dépendance du projet | ✅ (c'est le code) | ❌ (nouveau port vcpkg, composant compilé) | ✅ **déjà utilisé partout** |
| Volume de code à maintenir | ~1600 LOC bas niveau | Moyen | Moyen |
| Lisibilité | Faible (C-style) | Bonne | Bonne (C++ moderne) |
| Risque de migration (parité Nagios) | **Nul** (rien ne bouge) | Élevé | Élevé |
| Source du fuseau | zoneinfo via libc | POSIX TZ strings / CSV bundlé | **tzdata système IANA** |

## 6. Synthèse par option

### Code actuel
À garder si l'objectif est « utilisable dans broker à risque minimal ». Zéro
changement de comportement, suite de tests `get_next_valid_time/*` déjà verte,
aucune dépendance. Mais DST manuel/fragile, et surtout le fuseau via
`setenv/tzset` global → **inacceptable tel quel dans broker multi-threadé**. Le
réutiliser dans broker impose au minimum de remplacer `timezone_manager` par un
calcul sans état global.

### Boost.Date_Time
Le meilleur *vocabulaire* calendaire (`nth_kday_of_month`, iterators de dates) ;
c'est l'option qui raccourcit le plus la logique « nème jour du mois ». Mais
**nouvelle dépendance** (composant compilé, port vcpkg à ajouter) et modèle de
fuseau (`local_time` via POSIX TZ strings / CSV) plus daté et moins propre que la
tzdata d'Abseil. Ne se justifie que si ce vocabulaire prêt-à-l'emploi fait
réellement gagner.

### Abseil
Le meilleur compromis pour un *from scratch* : **déjà dépendance** (zéro coût
d'intégration — `absl::flat_hash_map` est déjà utilisé jusque dans
`timeperiod.hh`), **meilleure gestion DST** du lot (gestion explicite des instants
sautés/ambigus, exactement les cas `dst_forward`/`dst_backward`), **fuseau par
objet thread-safe** (`absl::TimeZone`, vraie tzdata IANA) qui élimine le
`setenv/tzset` global. Seul « moins » vs Boost : pas de helper `nth_kday` tout
fait → quelques lignes composées à partir de `CivilDay` + `GetWeekday`,
négligeable.

## 7. Recommandation

Le choix est essentiellement binaire selon l'objectif :

- **Objectif = découpler vite et réutiliser dans broker sans risque** → garder le
  code actuel, l'extraire en lib (cf. §8), mais **remplacer impérativement le
  `setenv/tzset` par un calcul de fuseau sans état global** — quitte à emprunter
  `absl::TimeZone` juste pour cette brique, sans réécrire le moteur. Risque
  comportemental nul, broker-safe.

- **Objectif = refaire de zéro proprement et durablement** → **Abseil**. Déjà
  présent, DST correct, fuseau par-timeperiod thread-safe ; et le proto porte déjà
  `timezone` / `use_timezone` (le besoin existe donc dans le modèle de données).

Boost ne gagne dans aucun scénario réaliste ici : Abseil est déjà dans le projet
et sa tzdata est meilleure.

> **Élément décisif** : le passage par `setenv("TZ")` + `tzset()` est un état
> global de process, thread-unsafe. Acceptable dans engine (mono-threadé sur ce
> chemin), il est dangereux dans broker (pools de threads io_context). C'est le
> vrai moteur de la décision, plus que le confort syntaxique.

## 8. Plan d'extraction

Ordre suggéré, du moins risqué au plus risqué :

1. Déplacer `timeperiod` / `daterange` / `timerange` (fait, sous
   `engine/src/timeperiods/`), corriger tous les includes consommateurs.
2. Supprimer les includes/usages morts (`broker.hh`, `using ...applier`).
3. Casser les couplages : logger injecté, `engine_error` → `msg_fmt`,
   `contains_illegal_object_chars` descendu, `daterange.hh` sans
   `engine/common.hh`.
4. Décision structurante : sortir le **registre global statique**
   `timeperiod::timeperiods`. Deux options :
   - **A — lib de logique pure** : la map reste chez chaque consommateur ;
     `resolve()` reçoit la map. Le plus propre/réutilisable, mais touche la
     signature et le code engine qui s'appuie sur `timeperiod::timeperiods`.
   - **B — lib + manager** : la lib fournit un petit `timeperiod_manager` que
     chaque daemon instancie. Moins de churn côté engine, plus de surface d'API.
5. Déplacer la lib sous `common/timeperiods/`, namespace
   `com::centreon::common::timeperiods` (garder un alias de transition côté engine
   pendant la migration des ~25 fichiers consommateurs).
6. Lier broker contre la lib pour l'évaluation des plages de notification.

## 9. Pistes à explorer

- **POC Abseil** : réécrire une fonction clé (p.ex. `get_next_valid_time` pour la
  règle « nème jour de semaine du mois ») en Abseil et la valider contre la suite
  `get_next_valid_time/*` existante, pour mesurer le gain de lisibilité et la
  robustesse DST.
- **Remplacement de `timezone_manager`** par un calcul de fuseau sans état global
  (brique prioritaire pour toute réutilisation dans broker).
- **Choix A vs B** pour le registre (cf. §8.4).
