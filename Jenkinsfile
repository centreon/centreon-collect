stage('Source') {
  node {
    sh 'setup_centreon_build.sh'
    dir('centreon-clib') {
      checkout scm
    }
    sh './centreon-build/jobs/clib/18.9/mon-clib-source.sh'
    source = readProperties file: 'source.properties'
    env.VERSION = "${source.VERSION}"
    env.RELEASE = "${source.RELEASE}"
  }
}

try {
  stage('Package') {
    parallel 'centos7': {
      node {
        sh 'setup_centreon_build.sh'
        sh './centreon-build/jobs/clib/18.9/mon-clib-package.sh centos7'
      }
    },
    'debian9': {
      node {
        sh 'setup_centreon_build.sh'
        sh './centreon-build/jobs/clib/18.9/mon-clib-package.sh debian9'
      }
    },
    'debian9-armhf': {
      node {
        sh 'setup_centreon_build.sh'
        sh './centreon-build/jobs/clib/18.9/mon-clib-package.sh debian9-armhf'
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
