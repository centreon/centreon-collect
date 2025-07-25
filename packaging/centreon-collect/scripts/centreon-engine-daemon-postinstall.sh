#!/bin/bash

log_info()  { 
  local context="${1:-}"
  shift
  if [[ -n "$context" ]]; then
    echo -e "\033[1;34m[INFO]\033[0m  [$context] $*"
  else
    echo -e "\033[1;34m[INFO]\033[0m  $*"
  fi
}

log_error() { 
  local context="${1:-}"
  shift
  if [[ -n "$context" ]]; then
    echo -e "\033[1;31m[ERROR]\033[0m [$context] $*" >&2
  else
    echo -e "\033[1;31m[ERROR]\033[0m $*" >&2
  fi
}

patch_whitelist() {
  local CONFIG_FILE="$1"
  local TMP_FILE
  TMP_FILE="$(mktemp)"

  #────────────────────────
  # Skip if already patched
  #────────────────────────
  if grep -q 'cma-whitelist' "$CONFIG_FILE"; then
    log_info "$CONFIG_FILE" "'cma-whitelist' already present. Skipping."
    return 0
  fi

  #──────────────────────────────────────────
  # Verify file format and whitelist presence
  #──────────────────────────────────────────
  local first_line
  first_line=$(head -n 1 "$CONFIG_FILE")
  
  if [[ "$first_line" =~ ^\{ ]] && grep -qE '\s+"whitelist":\s+\{' "$CONFIG_FILE"; then
    FORMAT="json"
  elif grep -qE '^[[:space:]]*whitelist:' "$CONFIG_FILE"; then
    FORMAT="yaml"
  else
    log_info "$CONFIG_FILE" "File does not contain expected whitelist structure. Skipping."
    return 0
  fi

  #───────────
  # JSON PATCH
  #───────────
  
  if [[ "$FORMAT" == "json" ]]; then

    extract_json() {
      local key="$1"
      local -n out_array="$2"
      out_array=()
      local capture=0

      while IFS= read -r line; do
        line="${line//[$'\t\r\n']}"
        [[ $capture -eq 1 ]] && {
          [[ "$line" == *"]"* ]] && break
          [[ "$line" =~ \"([^\"]*)\" ]] && out_array+=("\"${BASH_REMATCH[1]}\"")
        }
        [[ "$line" =~ \"${key}\"[[:space:]]*:[[:space:]]*\[* ]] && capture=1
      done < "$CONFIG_FILE"
    }

    extract_json "regex" json_regex
    extract_json "wildcard" json_wildcard

    if [[ ${#json_regex[@]} -eq 0 && ${#json_wildcard[@]} -eq 0 ]]; then
      log_info "$CONFIG_FILE" "No entries found in JSON whitelist. Skipping."
      return 0
    fi

  { 
    echo '{'
    echo '  "whitelist": {'

    # whitelist.regex
    if [ "${#json_regex[@]}" -ne 0 ]; then
      echo '    "regex": ['
      for i in "${!json_regex[@]}"; do
        comma=","
        [[ $i -eq $((${#json_regex[@]} - 1)) ]] && comma=""
        printf '      %s%s\n' "${json_regex[$i]}" "$comma"
      done
      echo -n '    ]'
      [[ "${#json_wildcard[@]}" -ne 0 ]] && echo ',' || echo
    fi

    # whitelist.wildcard (optional)
    if [ "${#json_wildcard[@]}" -ne 0 ]; then
      echo '    "wildcard": ['
      for i in "${!json_wildcard[@]}"; do
        comma=","
        [[ $i -eq $((${#json_wildcard[@]} - 1)) ]] && comma=""
        printf '      %s%s\n' "${json_wildcard[$i]}" "$comma"
      done
      echo '    ]'
    fi

    echo '  },'
    echo '  "cma-whitelist": {'
    echo '    "default": {'

    # cma-whitelist.regex
    if [ "${#json_regex[@]}" -ne 0 ]; then
      echo '      "regex": ['
      for i in "${!json_regex[@]}"; do
        comma=","
        [[ $i -eq $((${#json_regex[@]} - 1)) ]] && comma=""
        printf '        %s%s\n' "${json_regex[$i]}" "$comma"
      done
      echo -n '      ]'
      [[ "${#json_wildcard[@]}" -ne 0 ]] && echo ',' || echo
    fi

    # cma-whitelist.wildcard (optional)
    if [ "${#json_wildcard[@]}" -ne 0 ]; then
      echo '      "wildcard": ['
      for i in "${!json_wildcard[@]}"; do
        comma=","
        [[ $i -eq $((${#json_wildcard[@]} - 1)) ]] && comma=""
        printf '        %s%s\n' "${json_wildcard[$i]}" "$comma"
      done
      echo '      ]'
    fi

    echo '    }'
    echo '  }'
    echo '}'
  } > "$TMP_FILE"


  #───────────
  # YAML PATCH
  #───────────
  elif [[ "$FORMAT" == "yaml" ]]; then

    extract_yaml_array() {
      local key="$1"
      local -n out_array="$2"
      out_array=()

      local in_key=0

      while IFS= read -r line || [[ -n "$line" ]]; do
        # Trim whitespace
        line="${line#"${line%%[![:space:]]*}"}"
        line="${line%"${line##*[![:space:]]}"}"

        # Start of target array
        if [[ "$line" == "$key:" ]]; then
          in_key=1
          continue
        fi

        # Capture list items
        if [[ $in_key -eq 1 ]]; then
          if [[ "$line" =~ ^- ]]; then
            out_array+=("${line#- }")
          elif [[ "$line" =~ ^[a-zA-Z0-9_-]+: ]]; then
            break  # New key starts
          fi
        fi
      done < "$CONFIG_FILE"
    }

    extract_yaml_array "regex" yaml_regex
    extract_yaml_array "wildcard" yaml_wildcard

    if [[ ${#yaml_regex[@]} -eq 0 && ${#yaml_wildcard[@]} -eq 0 ]]; then
      log_info "$CONFIG_FILE" "No entries found in YAML whitelist. Skipping."
      return 0
    fi

    write_yaml_array() {
      local indent="$1"
      local -n arr="$2"
      for item in "${arr[@]}"; do
        echo "${indent}- \"${item//\"/}\""
      done
    }

    {
      echo "whitelist:"

      if [ "${#yaml_regex[@]}" -ne 0 ]; then
        echo "  regex:"
        write_yaml_array "    " yaml_regex
      fi

      if [ "${#yaml_wildcard[@]}" -ne 0 ]; then
        echo "  wildcard:"
        write_yaml_array "    " yaml_wildcard
      fi

      echo "cma-whitelist:"
      echo "  default:"

      if [ "${#yaml_regex[@]}" -ne 0 ]; then
        echo "    regex:"
        write_yaml_array "      " yaml_regex
      fi

      if [ "${#yaml_wildcard[@]}" -ne 0 ]; then
        echo "    wildcard:"
        write_yaml_array "      " yaml_wildcard
      fi
    } > "$TMP_FILE"

  fi

  mv "$TMP_FILE" "$CONFIG_FILE" || {
    log_error "$CONFIG_FILE" "Failed to overwrite $CONFIG_FILE"
    return 1
  }
}

patch_folder_whitelist(){
  # Check if the directory exists
  if [ ! -d /etc/centreon-engine-whitelist ]; then
    log_info "" "Nothing to patch, /etc/centreon-engine-whitelist does not exist."
    return 0
  fi
  log_info "" "Updating centreon-engine whitelist format"
  # recursively loop through all files in the /etc/centreon-engine-whitelist directory
  while IFS= read -r -d '' file; do
    patch_whitelist "$file"
  done < <(find /etc/centreon-engine-whitelist -type f -print0)

  return 0
}

startCentengine() {
  systemctl daemon-reload ||:
  systemctl unmask centengine.service ||:
  systemctl preset centengine.service ||:
  systemctl enable centengine.service ||:
  systemctl restart centengine.service ||:
}

debianLinkNagios() {
  if [ ! -r /usr/lib64/nagios/plugins ]; then
    if [ ! -d /usr/lib64/nagios ]; then
      mkdir -p /usr/lib64/nagios
      chmod 0755 /usr/lib64/nagios
    fi
    ln -s /usr/lib/nagios/plugins /usr/lib64/nagios/plugins
  fi
}

# on debian, it is needed to recreate centreon-engine user at each upgrade because it is removed on postrm step on versions < 23.10
if [ "$1" = "configure" ] ; then
  if [ ! "$(getent passwd centreon-engine)" ]; then
    adduser --system --group --home /var/lib/centreon-engine --shell /bin/bash --no-create-home centreon-engine
  fi
  if [ "$(getent passwd centreon-broker)" ]; then
    usermod -a -G centreon-engine centreon-broker
    usermod -a -G centreon-broker centreon-engine
  fi
  if [ "$(getent passwd centreon-gorgone)" ]; then
    usermod -a -G centreon-engine centreon-gorgone
    usermod -a -G centreon-gorgone centreon-engine
  fi
  if [ "$(getent passwd www-data)" ]; then
    usermod -a -G centreon-engine www-data
  fi
  if [ "$(getent passwd nagios)" ]; then
    usermod -a -G centreon-engine nagios
  fi
  chown -R centreon-engine:centreon-engine \
    /etc/centreon-engine \
    /var/lib/centreon-engine \
    /var/log/centreon-engine
  chmod -R g+w \
    /etc/centreon-engine \
    /var/log/centreon-engine
fi

action="$1"
if  [ "$1" = "configure" ] && [ -z "$2" ]; then
  # Alpine linux does not pass args, and deb passes $1=configure
  action="install"
elif [ "$1" = "configure" ] && [ -n "$2" ]; then
  # deb passes $1=configure $2=<current version>
  action="upgrade"
fi

#In debian nagios plugins are stored in /usr/lib/nagios/plugins instead of /usr/lib64/nagios/plugins
#so we create a link /usr/lib/nagios/plugins instead => /usr/lib64/nagios/plugins in order to have
#the same commands configuration for all pollers
if  [ "$1" = "configure" ]; then
  debianLinkNagios
fi

case "$action" in
  "1" | "install")
    startCentengine
    ;;
  "2" | "upgrade")
    patch_folder_whitelist
    startCentengine
    ;;
  *)
    # $1 == version being installed
    startCentengine
    ;;
esac


