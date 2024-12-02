
## Build Gorgone from sources

The up-to-date list of dependency of gorgone is available in this [file](../packaging/centreon-gorgone.yaml)

### centos 7
Using Github project, execute the following command to retrieve Gorgone source code:

```bash
git clone https://github.com/centreon/centreon-collect/
cd centreon-collect/gorgone
```

The daemon uses the following Perl modules:

* Repository 'centreon-stable':
  * ZMQ::LibZMQ4
  * UUID
  * Digest::MD5::File
* Repository 'centos base':
  * JSON::PP
  * JSON::XS
  * YAML
  * DBD::SQLite
  * DBD::mysql
  * Crypt::CBC
  * HTTP::Daemon
  * HTTP::Status
  * MIME::Base64
  * NetAddr::IP
* Repository 'epel':
  * HTTP::Daemon::SSL
  * Schedule::Cron
* From offline packages:
  * Hash::Merge
  * YAML::XS
  * Crypt::Cipher::AES (module CryptX)
  * Crypt::PK::RSA (module CryptX)
  * Crypt::PRNG (module CryptX)

Execute the following commands to install them all:

```bash
yum install 'perl(JSON::PP)' 'perl(Digest::MD5::File)' 'perl(NetAddr::IP)' 'perl(Schedule::Cron)' 'perl(Crypt::CBC)' 'perl(ZMQ::LibZMQ4)' 'perl(JSON::XS)' 'perl(YAML)' 'perl(DBD::SQLite)' 'perl(DBD::mysql)' 'perl(UUID)' 'perl(HTTP::Daemon)' 'perl(HTTP::Daemon::SSL)' 'perl(HTTP::Status)' 'perl(MIME::Base64)'
yum install packaging/packages/perl-CryptX-0.064-1.el7.x86_64.rpm packaging/packages/perl-YAML-LibYAML-0.80-1.el7.x86_64.rpm packaging/packages/perl-Hash-Merge-0.300-1.el7.noarch.rpm packaging/packages/perl-Clone-Choose-0.010-1.el7.noarch.rpm
```

### centos 8

Using Github project, execute the following command to retrieve Gorgone source code:

```bash
git clone https://github.com/centreon/centreon-gorgone
```

The daemon uses the following Perl modules:

* Repository 'centos base':
  * JSON::PP
  * YAML
  * DBD::SQLite
  * DBD::mysql
  * HTTP::Status
  * MIME::Base64
  * NetAddr::IP
* Repository 'epel':
  * Crypt::CBC
  * HTTP::Daemon::SSL
  * Schedule::Cron
  * Hash::Merge
* From offline packages:
  * ZMQ::LibZMQ4
  * UUID
  * Digest::MD5::File
  * JSON::XS
  * HTTP::Daemon
  * YAML::XS
  * Crypt::Cipher::AES (module CryptX)
  * Crypt::PK::RSA (module CryptX)
  * Crypt::PRNG (module CryptX)

Execute the following commands to install them all:

```bash
dnf install packaging/packages/*.el8*.rpm
dnf install 'perl(Hash::Merge)' 'perl(JSON::PP)' 'perl(NetAddr::IP)' 'perl(Schedule::Cron)' 'perl(Crypt::CBC)' 'perl(YAML)' 'perl(DBD::SQLite)' 'perl(DBD::mysql)' 'perl(HTTP::Daemon::SSL)' 'perl(HTTP::Status)' 'perl(MIME::Base64)'
```
