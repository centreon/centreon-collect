import groovy.json.JsonSlurper

/*
** Variables.
*/
properties([buildDiscarder(logRotator(numToKeepStr: '50'))])
def serie = '21.10'
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
              tools: [[$class: 'GoogleTestType', pattern: 'broker.xml,clib.xml,engine.xml']]
            ])
          }
        }
        dir('centreon-collect') {
          // Run sonarQube analysis
          withSonarQubeEnv('SonarQubeDev') {
            sh "./script/ci/analysis.sh"
          }
        }
      }
    }
    if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
      error('Unit tests stage failure.');
    }
  }

  // sonarQube step to get qualityGate result
  stage('Quality gate') {
    node {
      def reportFilePath = "target/sonar/report-task.txt"
      def reportTaskFileExists = fileExists "${reportFilePath}"
      if (reportTaskFileExists) {
        echo "Found report task file"
        def taskProps = readProperties file: "${reportFilePath}"
        echo "taskId[${taskProps['ceTaskId']}]"
        timeout(time: 10, unit: 'MINUTES') {
          while (true) {
            sleep 10
            def taskStatusResult    =
            sh(returnStdout: true, script: "curl -s -X GET -u ${authString} \'${sonarProps['sonar.host.url']}/api/ce/task?id=${taskProps['ceTaskId']}\'")
            echo "taskStatusResult[${taskStatusResult}]"
            def taskStatus  = new JsonSlurper().parseText(taskStatusResult).task.status
            echo "taskStatus[${taskStatus}]"
            // Status can be SUCCESS, ERROR, PENDING, or IN_PROGRESS. The last two indicate it's
            // not done yet.
            if (taskStatus != "IN_PROGRESS" && taskStatus != "PENDING") {
              break;
            }
            def qualityGate = waitForQualityGate()
            if (qualityGate.status != 'OK') {
              currentBuild.result = 'FAIL'
            }
          }
        }
      }
      if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
        error("Quality gate failure: ${qualityGate.status}.");
      }
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
