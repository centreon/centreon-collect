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
  - [10. Benchmarks — baseline actuelle](#10-benchmarks--baseline-actuelle)

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

## 9. Remplacement de `timezone_manager` — plan par phases

Le passage par `setenv("TZ")` + `tzset()` (cf. §3) est la vraie brique bloquante
pour broker. On le retire en trois phases, de la moins à la plus risquée :

- **Phase 0 — filet de sécurité (FAIT).** Avant de toucher au moteur, figer le
  comportement actuel par des tests golden-master multi-fuseaux :
  `common/tests/timeperiods/get_next_valid_time/timezone.cc`. Un même timeperiod
  « lundi 09:00-17:00 » évalué depuis un instant absolu unique donne, par fuseau,
  son 09:00 local (Paris `07:00 UTC` < UTC `09:00 UTC` < New_York `13:00 UTC`),
  avec un cas demi-heure (Australia/Lord_Howe, +10:30) pour piéger toute
  hypothèse d'offset entier. Un test `DISABLED_ConcurrentEvaluationsAre…`
  documente l'objectif : il ne pourra passer qu'une fois le fuseau devenu un
  paramètre par appel (impossible aujourd'hui, l'état TZ étant global). Jusque-là
  les tests posent le fuseau via `setenv/tzset` dans un garde RAII `scoped_tz`.
- **Phase 1 — fuseau en paramètre (FAIT).** `const absl::TimeZone& tz` traverse
  désormais toute la chaîne (`get_next_valid_time` / `check_time_against_period`
  / `*_per_timeperiod` + toutes les fonctions internes statiques). Deux helpers
  sans état remplacent libc dans `timeperiod.cc` : `tm_from_time(time_t, tz)`
  (= `localtime_r`, via `absl::ToTM`) et `time_from_tm(tm*, tz)` (= `mktime`, via
  `absl::FromTM` puis `absl::ToTM` pour réécrire les champs normalisés). DST :
  `absl::FromTM` renvoie l'instant pré-transition quand `tm_isdst != 0`, ce qui
  reproduit exactement `mktime` avec `tm_isdst = -1` (vérifié : tous les
  call-sites internes posent `tm_isdst = -1` avant conversion). **Compat engine
  sans toucher aux ~40 call-sites** : le paramètre a pour défaut
  `absl::LocalTimeZone()`, qui relit `TZ` à chaque appel (vérifié) → le chemin
  par défaut reproduit à l'identique le comportement actuel, y compris le
  `timezone_locker` global. Les consommateurs (broker, tests) passent un
  `absl::TimeZone` explicite → calcul thread-safe et sans état global. Le test
  concurrent de la Phase 0 est activé et vert. Bench inchangé sous `TZ=UTC`
  (gnvt_24x7 ~0.6 µs ; `LocalTimeZone()` résolu une seule fois par appel public
  puis la zone immuable est réutilisée pour toutes les conversions internes).
- **Phase 2 — suppression** de `timezone_manager` / `timezone_locker` une fois
  les call-sites engine migrés vers une zone explicite (`absl::LoadTimeZone(
  get_timezone())` au lieu de poser le locker). Bénéfice annexe : disparition du
  `stat()` de `/etc/localtime` à chaque appel (le ×4–×8 mesuré en §10).

Rappel : le fuseau n'est **pas** une propriété du timeperiod (le message proto
`Timeperiod` n'a pas de champ tz). Il vient du contexte (host/service/contact,
`get_timezone()`) ; c'est donc un paramètre d'évaluation, pas un membre.

Autres pistes :

- **POC Abseil** : réécrire une fonction clé (p.ex. `get_next_valid_time` pour la
  règle « nème jour de semaine du mois ») en Abseil et la valider contre la suite
  `get_next_valid_time/*` existante, pour mesurer le gain de lisibilité et la
  robustesse DST.
- **Choix A vs B** pour le registre (cf. §8.4).

## 10. Benchmarks — baseline actuelle

Des micro-benchmarks google-benchmark mesurent les deux fonctions chaudes du
scheduler (`check_time_against_period` et `get_next_valid_time`) sur des formes
de timeperiod représentatives. Source : `engine/benchmark/timeperiod.cc`
(cible CMake `timeperiod_bench`), liée uniquement à `timeperiods` + `engine_conf`.

```bash
ninja -Cbuild timeperiod_bench
./build/engine/benchmark/timeperiod_bench
```

**Méthodo / réserves** : mesures sur une machine de dev (22 cœurs @ 4.5 GHz,
build `-O2`), *CPU scaling activé* → à lire en **ordres de grandeur**. Médianes
sur 10 répétitions. Instants de référence fixes et déterministes (mer.
2024-01-03, sam. 2024-01-06) ; logging routé vers un null-sink. Le bench **fixe
`TZ=UTC`** (`setenv`+`tzset` dans son `main`) — voir l'encadré ci-dessous : sans
ça les chiffres explosent et deviennent ininterprétables.

| Benchmark | Forme / cas | Temps |
|---|---|---|
| `BM_check_24x7` | 24x7, instant valide | ~0.66 µs |
| `BM_check_workhours_inside` | lun-ven 9h-17h, dans la plage | ~0.63 µs |
| `BM_check_workhours_outside` | lun-ven 9h-17h, hors plage | ~1.1 µs |
| `BM_check_exceptions` | 3ᵉ lundi du mois (`month_week_day`) | ~8 µs |
| `BM_gnvt_24x7` | 24x7 | ~0.59 µs |
| `BM_gnvt_workhours_immediate` | déjà valide | ~0.58 µs |
| `BM_gnvt_workhours_search` | week-end → scan avant | ~0.58 µs |
| `BM_gnvt_exceptions` | prochain « 3ᵉ lundi » | **~120 µs** |
| `BM_gnvt_exclusion` | work-hours excluant 1 jour | ~1.5 µs |
| `BM_gnvt_exclusion_chain/1` | chaîne d'exclusions, profondeur 1 | ~1.5 µs |
| `BM_gnvt_exclusion_chain/2` | profondeur 2 | ~16 µs |
| `BM_gnvt_exclusion_chain/4` | profondeur 4 | ~66 µs |
| `BM_gnvt_exclusion_chain/8` | profondeur 8 | **~524 µs** |
| `BM_gnvt_exclusion_chain/16` | profondeur 16 | **~9 ms** |

**Enseignements :**

- Les cas simples (24x7, work-hours) coûtent ~0.6 µs sous `TZ` fixe.
- Le chemin **exceptions** (`month_week_day`) est 2 ordres de grandeur plus lent
  (~8 µs pour le `check`, **~120 µs** pour `get_next_valid_time`) : la recherche
  jour-par-jour combinée au calcul de date par jour candidat coûte cher.
- La **chaîne d'exclusions explose avec la profondeur** : à peu près ×8 par
  doublement (1.5 µs → 16 → 66 → 524 µs → **9 ms** à profondeur 16). La
  récursivité d'exclusion combine multiplicativement les scans jour-par-jour de
  chaque niveau → coût quasi géométrique. Argument fort en faveur du refactor :
  des exclusions profondes sont aujourd'hui pathologiques.

> **Sensibilité critique au `TZ`** : `get_next_valid_time`/`check_time_against_period`
> passent par `localtime_r`/`mktime`. Quand `TZ` n'est pas positionné, la glibc
> fait un `stat("/etc/localtime")` à *chaque* appel → **×4 à ×8** sur tous les
> chiffres (ex. `gnvt_24x7` : 0.46 µs sous `TZ=UTC`, 0.56 µs sous
> `TZ=Europe/Paris`, **2.0 µs `TZ` non défini** ; `gnvt_workhours` : 0.58 µs →
> 1.0 µs → **3.8 µs**). C'est pourquoi le bench fixe `TZ`. Au-delà du bench, c'est
> un **enseignement en soi** : la perf de l'implémentation actuelle est dominée
> par les fonctions temps de la libc et très sensible à l'environnement — un
> argument concret de plus pour Abseil, dont `absl::TimeZone` met le fuseau en
> cache (objet valeur, sans `stat` par appel ni état global).

Ces chiffres constituent la **baseline** (sous `TZ=UTC`) à comparer lors d'un
éventuel portage Abseil (cf. §9) — en particulier sur le chemin exceptions et la
chaîne d'exclusions.
