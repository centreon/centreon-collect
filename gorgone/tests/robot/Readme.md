## Robot tests for Gorgone

RobotFramework is an end to end test framework, used here to test Gorgone.

### setup containers

You need to have docker daemon up and running.

Every command consider you are at the racine of the centreon-collect repository, 
and there is an up to date centreon/centreon repository next to this one.
(this is used to set up the database, you need it only for the first start of the mariadb container, or when you purge it)

First you need to build the test image, we use bookworm and will tag it 'gorgone-bookworm', see .github/docker for other supported OS :
```
docker build --no-cache --file ./.github/docker/Dockerfile.gorgone-testing-bookworm --progress=plain -t gorgone-bookworm . 
```

Now you can launch it with a mariadb container, this is where you will run all tests.

Create the 2 container with a network for easy communication :
```
docker network create robot-gorgone-bookworm
docker run --env MYSQL_USER=centreon --env MYSQL_PASSWORD=password --env MARIADB_ROOT_PASSWORD=password --detach --network robot-gorgone-bookworm --name mariadb mariadb
# update the path to reflect where your repository are, this should only work on linux.
docker run --network robot-gorgone-bookworm --name 'gorgone-bookworm' -v $(pwd):/centreon-collect/:rw  -v $(pwd)/../centreon:/centreon/:rw -it  gorgone-bookworm bash
```

Connect to the robot bookworm container, go to the mount point, and create the database needed : 
```
cd /

# please check .github/workflows/gorgone.yml file for any update on this.

mysql -h mariadb -u root -ppassword -e "CREATE DATABASE \`centreon\`"
mysql -h mariadb -u root -ppassword -e "CREATE DATABASE \`centreon-storage\`"
mysql -h mariadb -u root -ppassword -e "GRANT ALL PRIVILEGES ON centreon.* TO 'centreon'@'%'"
mysql -h mariadb -u root -ppassword -e "GRANT ALL PRIVILEGES ON  \`centreon-storage\`.* TO 'centreon'@'%'"
mysql -h mariadb -u root -ppassword 'centreon' < centreon/centreon/www/install/createTables.sql
mysql -h mariadb -u root -ppassword 'centreon-storage' < centreon/centreon/www/install/createTablesCentstorage.sql
# we install gorgone because we want all dependancy to come from the package manager to be sure they work.
# locally you should use the code in the repository, which is controlled in robot execution by the argument -v 'gorgone_binary:...'
# by default and on the CI the binary used is the one installed by the package manager (from freshly built package)
apt update
apt install -y centreon-gorgone
cd /centreon-collect/
```

### Execute all tests
Launch robot tests with parameters to connect to the db and use the local gorgone binary : 
```
robot --loglevel TRACE -v 'gorgone_binary:/centreon-collect/gorgone/gorgoned' -v 'DBHOST:mariadb' -v 'DBNAME:centreon' -v 'DBNAME_STORAGE:centreon-storage' -v 'DBUSER:centreon' gorgone/tests/robot/tests
```

### Filter tests by tags

You can use tag to run only some test with, for exemple running only the test that are considered to long to be executed on each PR :  `--include long_tests`
Or to filter them instead : `--exclude long_tests`.


### Restart the host

After a reboot, you don't have to redo everything before running tests again : 
```
docker start mariadb
docker start gorgone-bookworm
docker exec -it gorgone-bookworm bash
```

### debug

No magic here, but you want at least to install the following binary to look at the file and see gorgone status : 
```
apt install -y vim nano htop top
```

#### debug installation

Be careful that gorgone don't stop if a module failed to start, it will only put a message in the logs.
You should check both the central and the poller log to check if a module failed to start

Maybe you installed an old version of centreon-gorgone which don't have all the dependency, for exemple rrd and Mojo-ioloop-signal :
```
apt install -y librrds-perl libmojo-ioloop-signal-perl centreon-perl-libs-common
```

For centreon-perl-libs-common, it is not present in 24.10 repository. on alma use any newer repo (like 25.03). 
For debian if you can't get the package you could copy the files yourself in the docker container :
```bash
mkdir -p /usr/share/perl5/centreon/ && cp -r /centreon-collect/perl-libs/lib/centreon/common /usr/share/perl5/centreon/
```

#### perl Debug

You can use perl debug mechanism on gorgone to add breakpoint or to see the code coverage : 
```bash
apt install -y cpanminus
cpanm Devel::Cover Devel::Camelcadedb
```

To see the code coverage, run your robot test with this variable environment : 
`PERL5OPT=-MDevel::Cover`

To use Intellij debugger, you can use this in your docker container to make the container listen on port 7765. 
Then use your ide to connect to the ip 127.0.0.1:7765 :

```bash
GORGONENAME='push_zmq_gorgone_central' ; PERL5_DEBUG_HOST=0.0.0.0 PERL5_DEBUG_PORT=7765 PERL5_DEBUG_ROLE=server perl -d:Camelcadedb /centreon-collect/gorgone/gorgoned --config="/etc/centreon-gorgone/$GORGONENAME/includer.yaml" --severity='debug'
```

You should probably run another gorgone without the debugger to act as a poller (or central if you want to debug the poller).
Configuration is generated by robot tests.