/*
** Variables.
*/
properties([buildDiscarder(logRotator(numToKeepStr: '50'))])
def serie = '20.10'
def maintenanceBranch = "${serie}.x"
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
    dir('centreon-collect') {
      // Checkout sources.
      checkout scm

      // Get version.
      def major = sh returnStdout: true, script: "grep 'set(COLLECT_MAJOR' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1"
      def minor = sh returnStdout: true, script: "grep 'set(COLLECT_MINOR' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1"
      def patch = sh returnStdout: true, script: "grep 'set(COLLECT_PATCH' CMakeLists.txt | cut -d ' ' -f 2 | cut -d ')' -f 1"
      env.VERSION = "${major}.${minor}.${patch}"

      // Get release number.
      if (env.BUILD == 'RELEASE') {
        env.RELEASE = env.BUILD_NUMBER
      } else {
        now = sh returnStdout: true, script: 'date +%s'
        commit = sh returnStdout: true, script: 'git log -1 HEAD --pretty=format:%h'
        env.RELEASE = "${now}.${commit}"
      }
    }
    sh 'tar czf centreon-collect.tar.gz centreon-collect'
    stash name: 'git-sources', includes: 'centreon-collect.tar.gz'
  }
}

try {
  stage('Unit tests') {
    parallel 'centos7': {
      node {
        unstash 'git-sources'
        sh 'rm -rf centreon-collect && tar xzf centreon-collect.tar.gz'
        def utImage = 'registry.centreon.com/collect-build-deps-20.10:centos7'
        docker.image(utImage).pull()
        docker.image(utImage).inside() {
          dir('centreon-collect') {
            sh './script/ci/ut.sh'
            step([
              $class: 'XUnitBuilder',
              thresholds: [
                [$class: 'FailedThreshold', failureThreshold: '0'],
                [$class: 'SkippedThreshold', failureThreshold: '0']
              ],
              tools: [[$class: 'GoogleTestType', pattern: 'ut.xml']]
            ])
            if ((env.BUILD == 'RELEASE') || (env.BUILD == 'REFERENCE')) {
              withSonarQubeEnv('SonarQube') {
                sh 'sonar-scanner'
              }
            }
          }
        }
      }
    }
    if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
      error('Unit tests stage failure.');
    }
  }

  stage('Package') {
    parallel 'centos7': {
      node {
        unstash 'git-sources'
        sh 'rm -rf centreon-collect && tar xzf centreon-collect.tar.gz'
        def packageImage = 'registry.centreon.com/collect-build-deps-20.10:centos7'
        docker.image(packageImage).pull()
        docker.image(packageImage).inside() {
          dir('centreon-collect') {
            sh './script/ci/package.sh centos7'
          }
        }
      }
    }
    if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
      error('Package stage failure.');
    }
  }
}
finally {
  buildStatus = currentBuild.result ?: 'SUCCESS';
  if ((buildStatus != 'SUCCESS') && ((env.BUILD == 'RELEASE') || (env.BUILD == 'REFERENCE'))) {
    slackSend channel: '#monitoring-metrology', message: "@channel Centreon Collect build ${env.BUILD_NUMBER} of branch ${env.BRANCH_NAME} is broken. Please fix it ASAP."
  }
}
