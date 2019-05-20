/*
** Variables.
*/
def serie = '19.10'
def maintenanceBranch = "${serie}"
if (env.BRANCH_NAME.startsWith('release-')) {
  env.BUILD = 'RELEASE'
} else if ((env.BRANCH_NAME == 'master') || (env.BRANCH_NAME == maintenanceBranch)) {
  env.BUILD = 'REFERENCE'
} else {
  env.BUILD = 'CI'
}

/*
** Pipeline code.
*/
stage('Source') {
  node {
    sh 'setup_centreon_build.sh'
    dir('centreon-clib') {
      checkout scm
    }
    sh "./centreon-build/jobs/clib/${serie}/mon-clib-source.sh"
    source = readProperties file: 'source.properties'
    env.VERSION = "${source.VERSION}"
    env.RELEASE = "${source.RELEASE}"
    publishHTML([
      allowMissing: false,
      keepAll: true,
      reportDir: 'summary',
      reportFiles: 'index.html',
      reportName: 'Centreon Clib Build Artifacts',
      reportTitles: ''
    ])
    if ((env.BUILD == 'RELEASE') || (env.BUILD == 'REFERENCE')) {
      withSonarQubeEnv('SonarQube') {
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-analysis.sh"
      }
    }
  }
}

try {
  stage('Package') {
    parallel 'centos7': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh centos7"
      }
    },
    'debian9': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh debian9"
      }
    },
    'debian9-armhf': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh debian9-armhf"
      }
    },
    'debian10': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh debian10"
      }
    /*
    },
    'opensuse-leap': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh opensuse-leap"
      }
    */
    }
    if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
      error('Package stage failure.');
    }
  }

  if ((env.BUILD == 'RELEASE') || (env.BUILD == 'REFERENCE')) {
    stage('Delivery') {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-delivery.sh"
      }
      if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
        error('Delivery stage failure.');
      }
    }
  }
}
finally {
  buildStatus = currentBuild.result ?: 'SUCCESS';
  if ((buildStatus != 'SUCCESS') && ((env.BUILD == 'RELEASE') || (env.BUILD == 'REFERENCE'))) {
    slackSend channel: '#monitoring-metrology', message: "@channel Centreon Clib build ${env.BUILD_NUMBER} of branch ${env.BRANCH_NAME} was broken by ${source.COMMITTER}. Please fix it ASAP."
  }
}
