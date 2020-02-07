FROM buildkite/puppeteer:latest
RUN apt-get update && apt-get install -y build-essential git python && apt-get clean all
