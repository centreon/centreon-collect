stage('Source') {
  node {
    sh 'cd /opt/centreon-build && git pull && cd -'
    dir('centreon-clib') {
      checkout scm
    }
    sh '/opt/centreon-build/jobs/clib/3.4/mon-clib-source.sh'
    source = readProperties file: 'source.properties'
    env.VERSION = "${source.VERSION}"
    env.RELEASE = "${source.RELEASE}"
  }
}

try {
  stage('Package') {
    parallel 'centos6': {
      node {
        sh 'cd /opt/centreon-build && git pull && cd -'
        sh '/opt/centreon-build/jobs/clib/3.4/mon-clib-package.sh centos6'
      }
    },
    'centos7': {
      node {
        sh 'cd /opt/centreon-build && git pull && cd -'
        sh '/opt/centreon-build/jobs/clib/3.4/mon-clib-package.sh centos7'
      }
    },
    'debian9': {
      node {
        sh 'cd /opt/centreon-build && git pull && cd -'
        sh '/opt/centreon-build/jobs/clib/3.4/mon-clib-package.sh debian9'
      }
    }
    if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
      error('Package stage failure.');
    }
  }
}
finally {
  buildStatus = currentBuild.result ?: 'SUCCESS';
  if ((buildStatus != 'SUCCESS') && (env.BRANCH_NAME == 'master')) {
    slackSend channel: '#monitoring-metrology', message: "@channel Centreon Clib build ${env.BUILD_NUMBER} of branch ${env.BRANCH_NAME} was broken by ${source.COMMITTER}. Please fix it ASAP."
  }
}
