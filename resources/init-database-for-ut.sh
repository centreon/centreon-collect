docker run --name mariadbtest -e MYSQL_ROOT_PASSWORD=centreon -p 3306:3306 -d docker.io/library/mariadb:10.5
mariadb -h 127.0.0.1 -u root -pcentreon < ../resources/centreon_storage.sql
mariadb -h 127.0.0.1 -u root -pcentreon < ../resources/centreon.sql
