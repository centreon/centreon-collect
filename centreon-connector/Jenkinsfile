/*
** Variables.
*/
properties([buildDiscarder(logRotator(numToKeepStr: '10'))])
def serie = '21.10'
def maintenanceBranch = "${serie}.x"
def qaBranch = "dev-${serie}.x"

if (env.BRANCH_NAME.startsWith('release-')) {
  env.BUILD = 'RELEASE'
} else if ((env.BRANCH_NAME == 'master') || (env.BRANCH_NAME == maintenanceBranch)) {
  env.BUILD = 'REFERENCE'
} else if ((env.BRANCH_NAME == 'develop') || (env.BRANCH_NAME == qaBranch)) {
  env.BUILD = 'QA'
} else {
  env.BUILD = 'CI'
}

/*
** Functions
*/
def buildBranch = env.BRANCH_NAME
if (env.CHANGE_BRANCH) {
  buildBranch = env.CHANGE_BRANCH
}

def checkoutCentreonBuild(buildBranch) {
  def getCentreonBuildGitConfiguration = { branchName -> [
    $class: 'GitSCM',
    branches: [[name: "refs/heads/${branchName}"]],
    doGenerateSubmoduleConfigurations: false,
    userRemoteConfigs: [[
      $class: 'UserRemoteConfig',
      url: "ssh://git@github.com/centreon/centreon-build.git"
    ]]
  ]}

  dir('centreon-build') {
    try {
      checkout(getCentreonBuildGitConfiguration(buildBranch))
    } catch(e) {
      echo "branch '${buildBranch}' does not exist in centreon-build, then fallback to master"
      checkout(getCentreonBuildGitConfiguration('master'))
    }
  }
}

/*
** Pipeline code.
*/
stage('Source') {
  node("C++") {
    dir('centreon-connector') {
      checkout scm
    }
    checkoutCentreonBuild(buildBranch)
    sh "./centreon-build/jobs/connector/${serie}/mon-connector-source.sh"
    source = readProperties file: 'source.properties'
    publishHTML([
      allowMissing: false,
      keepAll: true,
      reportDir: 'summary',
      reportFiles: 'index.html',
      reportName: 'Centreon Connector Build Artifacts',
      reportTitles: ''
    ])
    env.VERSION = "${source.VERSION}"
    env.RELEASE = "${source.RELEASE}"
  }
}

try {
  stage('Packaging // Sonar analysis // Quality Gate') {
    parallel 'packaging centos7': {
      node("C++") {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/connector/${serie}/mon-connector-package.sh centos7"
        stash name: 'el7-rpms', includes: "output/x86_64/*.rpm"
        archiveArtifacts artifacts: "output/x86_64/*.rpm"
      }
    },
    'packaging centos8': {
      node("C++") {
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/connector/${serie}/mon-connector-package.sh centos8"
        stash name: 'el8-rpms', includes: "output/x86_64/*.rpm"
        archiveArtifacts artifacts: "output/x86_64/*.rpm"
      }
    },
    'Sonar analysis and Quality Gate': {
      node("C++") {
        sh 'setup_centreon_build.sh'
        withSonarQubeEnv('SonarQubeDev') {
          sh "./centreon-build/jobs/connector/${serie}/mon-connector-analysis.sh"
        }
      }  
    }
    timeout(time: 10, unit: 'MINUTES') {
      def qualityGate = waitForQualityGate()
      if (qualityGate.status != 'OK') {
        currentBuild.result = 'FAIL'
      }
    }
    if ((currentBuild.result ?: 'SUCCESS') != 'SUCCESS') {
      error('Packaging // Sonar analysis // Quality Gate failure');
    }
  }

  if ((env.BUILD == 'RELEASE') || (env.BUILD == 'QA')) {
    stage('Delivery') {
      node("C++") {
        unstash 'el7-rpms'
        unstash 'el8-rpms'
        sh 'setup_centreon_build.sh'
        sh "./centreon-build/jobs/connector/${serie}/mon-connector-delivery.sh"
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
    slackSend channel: '#monitoring-metrology', message: "@channel Centreon Connector build ${env.BUILD_NUMBER} of branch ${env.BRANCH_NAME} was broken by ${source.COMMITTER}. Please fix it ASAP."
  }
}
