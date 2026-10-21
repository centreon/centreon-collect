#!/bin/bash
# Show, from the server side, the sessions of the centreon user and their TLS cipher
# (empty cipher = clear connection).
cd "$(dirname "$0")"
docker compose exec -T db mariadb -uroot -proot -e "
    SELECT t.PROCESSLIST_ID AS id, t.PROCESSLIST_USER AS user, t.PROCESSLIST_HOST AS host,
           IFNULL(NULLIF(s.VARIABLE_VALUE, ''), '(clear)') AS ssl_cipher
    FROM performance_schema.threads t
    LEFT JOIN performance_schema.status_by_thread s
        ON s.THREAD_ID = t.THREAD_ID AND s.VARIABLE_NAME = 'Ssl_cipher'
    WHERE t.PROCESSLIST_USER = 'centreon'"
