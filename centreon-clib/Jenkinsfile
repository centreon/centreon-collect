import groovy.json.JsonSlurper

/*
** Variables.
*/
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
    withSonarQubeEnv('SonarQubeDev') {
      sh "./centreon-build/jobs/clib/${serie}/mon-clib-analysis.sh"
    }
  }
}

try {
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
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh centos7"
      }
    },
    'centos8': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh centos8"
      }
    },
    'debian10': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh debian10"
      }
    },
    'debian10-armhf': {
      node {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/clib/${serie}/mon-clib-package.sh debian10-armhf"
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
