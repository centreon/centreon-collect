@Library("centreon-shared-library")_

/*
** Variables.
*/

env.REF_BRANCH = 'master'
env.PROJECT='centreon-collect'
def serie = '22.04'
def maintenanceBranch = "${serie}.x"
def qaBranch = "dev-${serie}.x"
def buildBranch = env.BRANCH_NAME
if (env.CHANGE_BRANCH) {
  buildBranch = env.CHANGE_BRANCH
}

echo "DEBUG"
echo "env.BRANCH_NAME"
echo "${env.BRANCH_NAME}"

echo "env.GIT_BRANCH"
echo "${env.GIT_BRANCH_NAME}"

echo "env.GIT_LOCAL_BRANCH"
echo "${env.GIT_LOCAL_BRANCH}"

echo "env.CHANGE_BRANCH"
echo "${env.CHANGE_BRANCH}"

echo "env.CHANGE_ID"
echo "${env.CHANGE_ID}"


/*
** Branch management
*/
if (env.BRANCH_NAME.startsWith('release-')) {
  env.BUILD = 'RELEASE'
  env.REPO = 'testing'
} else if ((env.BRANCH_NAME == env.REF_BRANCH) || (env.BRANCH_NAME == maintenanceBranch)) {
  env.BUILD = 'REFERENCE'
} else if ((env.BRANCH_NAME == 'develop') || (env.BRANCH_NAME == qaBranch)) {
  env.BUILD = 'QA'
  env.REPO = 'unstable'
} else {
  env.BUILD = 'CI'
}

/*
** Pipeline code.
*/
stage('Deliver sources') {
  node("C++") {
    dir('centreon-collect-centos7') {
      checkout scm
      loadCommonScripts()
      sh 'ci/scripts/collect-sources-delivery.sh'
      source = readProperties file: 'source.properties'
      env.VERSION = "${source.VERSION}"
      env.RELEASE = "${source.RELEASE}"
    }
  }
}

stage('Build / Unit tests // Packaging / Signing') {
  parallel 'centos7 Build and UT': {
    node("C++") {
      dir('centreon-collect-centos7') {
        checkout scm
        sh 'docker run -i --entrypoint /src/ci/scripts/collect-unit-tests.sh -v "$PWD:/src" registry.centreon.com/centreon-collect-centos7-dependencies:22.04-testdocker'
      }
    }
  },
  'centos7 SQ analysis': {
    node("C++") {
      dir('centreon-collect-centos7') {
        checkout scm
        echo "DEBUG 2"
        sh 'ci/scripts/sonar-scanner.sh'
        withSonarQubeEnv('SonarQubeDev') {
          echo "DEBUG 3"
          sh 'printenv'
          //sh 'docker run -i --entrypoint /src/ci/scripts/collect-sources-analysis -v "$PWD:/src" registry.centreon.com/centreon-collect-centos7-dependencies:22.04-testdocker "$SONAR_AUTH_TOKEN" "$SONAR_HOST_URL"'
          sh 'docker run -i -v "$PWD:/src" -w="/src" --entrypoint ci/scripts/collect-sources-analysis --rm -u $(id -u):$(id -g) registry.centreon.com/centreon-collect-centos7-dependencies:22.04-testdocker "$SONAR_AUTH_TOKEN" "$SONAR_HOST_URL"'
        }
      }
    }
  }/*,
  'centos7 rpm packaging and signing': {
    node("C++") {
      dir('centreon-collect-centos7') {
        checkout scm
        sh 'docker run -i --entrypoint /src/ci/scripts/collect-rpm-package.sh -v "$PWD:/src" -e DISTRIB="el7" -e VERSION=$VERSION -e RELEASE=$RELEASE registry.centreon.com/centreon-collect-centos7-dependencies:22.04-testdocker'
        sh 'rpmsign --addsign *.rpm'
        stash name: 'el7-rpms', includes: '*.rpm'
        archiveArtifacts artifacts: "*.rpm"
        sh 'rm -rf *.rpm'
      } 
    }
  },
  'alma8 rpm packaging and signing': {
    node("C++") {
      dir('centreon-collect-alma8') {
        checkout scm
        sh 'docker run -i --entrypoint /src/ci/scripts/collect-rpm-package.sh -v "$PWD:/src" -e DISTRIB="el8" -e VERSION=$VERSION -e RELEASE=$RELEASE registry.centreon.com/centreon-collect-alma8-dependencies:22.04-testdocker'
        sh 'rpmsign --addsign *.rpm'
        stash name: 'el8-rpms', includes: '*.rpm'
        archiveArtifacts artifacts: "*.rpm"
        sh 'rm -rf *.rpm'
      }
    }
  },
  'debian buster Build and UT': {
    node("C++") {
      dir('centreon-collect-debian10') {
        checkout scm
        sh 'docker run -i --entrypoint /src/ci/scripts/collect-unit-tests.sh -v "$PWD:/src" registry.centreon.com/centreon-collect-debian10-dependencies:22.04'
      }
    }
  },
  'debian buster packaging and signing': {
    node("C++") {
      dir('centreon-collect') {
        checkout scm
      }
        sh 'docker run -i --entrypoint /src/centreon-collect/ci/scripts/collect-deb-package.sh -v "$PWD:/src" -e DISTRIB="Debian10" -e VERSION=$VERSION -e RELEASE=$RELEASE registry.centreon.com/centreon-collect-debian10-dependencies:22.04'
        stash name: 'Debian10', includes: 'Debian10/*.deb'
        archiveArtifacts artifacts: "Debian10/*"
    }
  },
  'debian bullseye Build and UT': {
    node("C++") {
      dir('centreon-collect-debian11') {
        checkout scm
        sh 'docker run -i --entrypoint /src/ci/scripts/collect-unit-tests.sh -v "$PWD:/src" registry.centreon.com/centreon-collect-debian11-dependencies:22.04'
      }
    }
  },
  'debian bullseye packaging and signing': {
    node("C++") {
      dir('centreon-collect') {
        checkout scm
      }
      sh 'docker run -i --entrypoint /src/centreon-collect/ci/scripts/collect-deb-package.sh -v "$PWD:/src" -e DISTRIB="Debian11" -e VERSION=$VERSION -e RELEASE=$RELEASE registry.centreon.com/centreon-collect-debian11-dependencies:22.04'
      stash name: 'Debian11', includes: 'Debian11/*.deb'
      archiveArtifacts artifacts: "Debian11/*"
    }
  }*/
}

stage('Quality Gate') {
  timeout(time: 10, unit: 'MINUTES') {
    waitForQualityGate()
  }
}

/*
stage('Delivery') {
  node("C++") {
    unstash 'el8-rpms'
    unstash 'el7-rpms'
    dir('centreon-collect-delivery') {
      checkout scm
      loadCommonScripts()
      sh 'rm -rf output && mkdir output && mv ../*.rpm output'
      sh './ci/scripts/collect-rpm-delivery.sh'
      withCredentials([usernamePassword(credentialsId: 'nexus-credentials', passwordVariable: 'NEXUS_PASSWORD', usernameVariable: 'NEXUS_USERNAME')]) {
        checkout scm
        unstash "Debian11"
        sh 'mv Debian11/*.deb .'
        sh '''for i in $(echo *.deb)
              do 
                curl -u $NEXUS_USERNAME:$NEXUS_PASSWORD -H "Content-Type: multipart/form-data" --data-binary "@./$i" https://apt.centreon.com/repository/22.04-$REPO/
              done
           '''    
      }
    }
  }
}*/
