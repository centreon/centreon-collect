FROM centos:7
RUN yum install -y bzip2 fontconfig tar wget
RUN wget https://bitbucket.org/ariya/phantomjs/downloads/phantomjs-2.1.1-linux-x86_64.tar.bz2
RUN tar xjf phantomjs-2.1.1-linux-x86_64.tar.bz2
RUN mv phantomjs-2.1.1-linux-x86_64/bin/phantomjs /usr/local/bin/phantomjs
ENTRYPOINT /usr/local/bin/phantomjs --webdriver=4444
