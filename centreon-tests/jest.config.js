module.exports = {
  "roots": [
    "<rootDir>/src"
  ],
  "testMatch": [
    "**/?(*.)+(spec|test).+(ts|tsx|js)"
  ],
  "transform": {
    "^.+\\.tsx?$": "ts-jest",
  },
  "preset": "ts-jest",
  "testEnvironment": "node",

}
