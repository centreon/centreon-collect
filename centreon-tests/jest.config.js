const { mergeDeepRight } = require('ramda');

module.exports = mergeDeepRight(require('@centreon/frontend-core/jest'), {
  roots: ['<rootDir>/src'],
  testEnvironment: 'node',
});
