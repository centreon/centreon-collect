package org.centreon

class Utils implements Serializable {
  def script

  Utils(script) {
    this.script = script

    this.script.env.BUILD_BRANCH = script.env.BRANCH_NAME
    if (this.script.env.CHANGE_BRANCH) {
      this.script.env.BUILD_BRANCH = script.env.CHANGE_BRANCH
    }
    this.script.println("Build branch is ${this.script.env.BUILD_BRANCH}")


    def matcher = script.env.CHANGE_URL =~ /github.com\/centreon\/(.+?)\//
    assert matcher instanceof java.util.regex.Matcher
    assert 1 == matcher.count
    this.script.env.GIT_PROJECT_NAME = matcher[0][1]
    this.script.println("Git project name is ${this.script.env.GIT_PROJECT_NAME}")
  }

  def checkoutCentreonBuild() {

    def getCentreonBuildGitConfiguration = { branchName -> [
      $class: 'GitSCM',
      branches: [[name: "refs/heads/${branchName}"]],
      doGenerateSubmoduleConfigurations: false,
      userRemoteConfigs: [[
        $class: 'UserRemoteConfig',
        url: "ssh://git@github.com/centreon/centreon-build.git"
      ]]
    ]}

    this.script.dir('centreon-build') {
      try {
        this.script.checkout(getCentreonBuildGitConfiguration(script.env.BUILD_BRANCH))
      } catch(e) {
        this.script.println("branch '${script.env.BUILD_BRANCH}' does not exist in centreon-build, then fallback to master")
        this.script.checkout(getCentreonBuildGitConfiguration('master'))
      }
    }
  }

  def sendViolationsToGithub() {
    if (this.script.env.CHANGE_ID) { // pull request to comment with coding style issues
      script.ViolationsToGitHub([
        repositoryName: this.script.env.GIT_PROJECT_NAME,
        pullRequestId: this.script.env.CHANGE_ID,

        createSingleFileComments: true,
        commentOnlyChangedContent: true,
        commentOnlyChangedFiles: true,
        keepOldComments: false,

        commentTemplate: "**{{violation.severity}}**: {{violation.message}}",

        violationConfigs: [
          [parser: 'CHECKSTYLE', pattern: '.*/codestyle-be.xml$', reporter: 'Checkstyle'],
          [parser: 'CHECKSTYLE', pattern: '.*/phpstan.xml$', reporter: 'Checkstyle'],
          [parser: 'CHECKSTYLE', pattern: '.*/codestyle-fe.xml$', reporter: 'Checkstyle']
        ]
      ])
    }
  }

  def checkSonarQualityGate() {
    def qualityGate = this.script.waitForQualityGate()
    if (qualityGate.status != 'OK') {
      this.script.error("Pipeline aborted due to quality gate failure: ${qualityGate.status}")
    }
  }
}
