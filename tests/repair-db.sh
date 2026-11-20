#!/usr/bin/env bash
#
# repair-db.sh — répare la base MariaDB de test quand elle refuse de démarrer.
#
# À LANCER DANS LE CONTENEUR (eloquent_margulis), en root :
#     cd /work/develop/tests/
#     ./repair-db.sh                 # réparation standard
#     ./repair-db.sh --reinit        # dernier recours : reconstruit le schéma
#     ./repair-db.sh --reinit --yes  # idem, sans confirmation
#
# --------------------------------------------------------------------------
# Cause la plus fréquente :
#   Des fichiers de /var/lib/mysql appartiennent à root:root au lieu de
#   mysql:mysql (typiquement après un arrêt brutal du conteneur). mariadbd
#   tourne sous l'utilisateur « mysql » et ne peut plus ouvrir ses tablespaces
#   InnoDB, d'où l'erreur :
#       [ERROR] InnoDB: Tablespace NNN was not found at ./<db>/<table>.ibd
#   La réparation standard se contente alors de remettre le bon propriétaire
#   sur le datadir, puis de redémarrer.
#
#   Si le datadir est réellement corrompu (et pas seulement mal possédé),
#   --reinit reconstruit la base de test à partir des schémas du dépôt via
#   init-sql.sh. ATTENTION : --reinit DÉTRUIT les données de la base de test.
# --------------------------------------------------------------------------

set -uo pipefail

DATADIR=/var/lib/mysql
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

REINIT=0
ASSUME_YES=0
for arg in "$@"; do
  case "$arg" in
    --reinit) REINIT=1 ;;
    --yes|-y) ASSUME_YES=1 ;;
    -h|--help)
      sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *) echo "Option inconnue : $arg (voir --help)" >&2; exit 2 ;;
  esac
done

log()  { printf '\033[1;34m[repair-db]\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m[repair-db]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[repair-db]\033[0m %s\n' "$*"; }
err()  { printf '\033[1;31m[repair-db]\033[0m %s\n' "$*" >&2; }

is_alive() { mysqladmin ping >/dev/null 2>&1; }

# 0. Doit être lancé en root (dans le conteneur).
if [[ "$(id -u)" -ne 0 ]]; then
  err "Ce script doit être lancé en root, à l'intérieur du conteneur."
  err "  podman exec -ti eloquent_margulis /bin/bash"
  err "  cd /work/develop/tests/ && ./repair-db.sh"
  exit 1
fi

# 1. Déjà démarrée ? (sauf si on force la réinitialisation)
if [[ "$REINIT" -eq 0 ]] && is_alive; then
  ok "MariaDB est déjà démarrée et répond. Rien à faire."
  exit 0
fi

# 2. Arrêter proprement une éventuelle instance résiduelle / bloquée.
log "Arrêt d'une éventuelle instance résiduelle…"
service mariadb stop >/dev/null 2>&1
pkill -9 mariadbd 2>/dev/null
pkill -9 mysqld   2>/dev/null
sleep 1

# 3. Corriger les permissions du datadir (cause n°1).
bad="$(find "$DATADIR" \( ! -user mysql -o ! -group mysql \) 2>/dev/null | wc -l)"
log "Fichiers mal possédés dans $DATADIR : $bad"
log "chown -R mysql:mysql $DATADIR …"
chown -R mysql:mysql "$DATADIR"

# 4. Démarrage.
log "Démarrage de MariaDB…"
start_output="$(service mariadb start 2>&1)"
sleep 2

if is_alive; then
  ok "MariaDB est repartie. ✅"
  if [[ "$REINIT" -eq 0 ]]; then
    exit 0
  fi
fi

# 5. Toujours pas démarrée → diagnostic, puis éventuelle réinitialisation.
if ! is_alive; then
  err "Le démarrage standard a échoué. Sortie :"
  printf '%s\n' "$start_output" | tail -n 20 >&2
  if [[ "$REINIT" -eq 0 ]]; then
    echo >&2
    warn "Si le datadir est corrompu (et pas seulement mal possédé), relance avec :"
    warn "    ./repair-db.sh --reinit        # DÉTRUIT et reconstruit la base de test"
    exit 1
  fi
fi

# 6. Réinitialisation (--reinit) : reconstruit le schéma via init-sql.sh.
#    Nécessite une instance démarrée. Si elle ne démarre pas du tout à cause
#    d'InnoDB, on repart d'un datadir neuf.
if [[ "$REINIT" -eq 1 ]]; then
  if [[ "$ASSUME_YES" -ne 1 ]]; then
    warn "--reinit va DÉTRUIRE les bases 'centreon' et 'centreon_storage' de test."
    read -r -p "Confirmer ? [oui/N] " answer
    case "$answer" in
      oui|o|y|yes|Y) ;;
      *) err "Annulé."; exit 1 ;;
    esac
  fi

  # Si MariaDB ne démarre toujours pas, on recrée un datadir vierge.
  if ! is_alive; then
    warn "MariaDB ne démarre pas : recréation d'un datadir InnoDB neuf."
    ts="$(date +%Y%m%d-%H%M%S)"
    log "Sauvegarde de l'ancien datadir → ${DATADIR}.broken-${ts}"
    service mariadb stop >/dev/null 2>&1
    pkill -9 mariadbd 2>/dev/null; pkill -9 mysqld 2>/dev/null; sleep 1
    mv "$DATADIR" "${DATADIR}.broken-${ts}"
    mkdir -p "$DATADIR"
    chown mysql:mysql "$DATADIR"
    log "mariadb-install-db …"
    mariadb-install-db --user=mysql --datadir="$DATADIR" >/dev/null 2>&1 \
      || mysql_install_db --user=mysql --datadir="$DATADIR" >/dev/null 2>&1
    chown -R mysql:mysql "$DATADIR"
    service mariadb start >/dev/null 2>&1
    sleep 2
  fi

  if ! is_alive; then
    err "Impossible de démarrer MariaDB même après recréation du datadir."
    exit 1
  fi

  log "Reconstruction du schéma de test (init-sql.sh)…"
  cd "$SCRIPT_DIR" || { err "cd $SCRIPT_DIR impossible"; exit 1; }
  if ./init-sql.sh; then
    ok "Base de test reconstruite et MariaDB opérationnelle. ✅"
    exit 0
  else
    err "init-sql.sh a échoué."
    exit 1
  fi
fi
