FROM ubuntu:rolling

# Install tools.
RUN apt-get update && apt-get -y install apt-transport-https unzip wget

# Install Chrome repositories.
RUN wget -q -O - https://dl-ssl.google.com/linux/linux_signing_key.pub | apt-key add -
RUN echo "deb [arch=amd64] https://dl.google.com/linux/chrome/deb/ stable main" > /etc/apt/sources.list.d/google-chrome.list

# Install Chrome, JRE (for Selenium) and xvfb (to run Chrome headless).
RUN apt-get update && apt-get -y install default-jre google-chrome-stable xvfb

# Install Selenium.
RUN wget https://goo.gl/dR7Lg2 -O selenium-server-standalone.jar

# Install Chrome WebDriver.
RUN wget https://chromedriver.storage.googleapis.com/2.35/chromedriver_linux64.zip
RUN unzip chromedriver_linux64.zip && mv chromedriver /usr/local/bin/

# Entrypoint.
COPY webdrivers/selenium.sh /usr/local/bin/selenium.sh
ENTRYPOINT ["/usr/local/bin/selenium.sh"]
