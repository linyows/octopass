FROM centos:latest
MAINTAINER linyows <linyows@gmail.com>

RUN yum install -y glibc gcc make libcurl-devel jansson-devel wget bzip2 git vim epel-release && \
    yum install -y clang
RUN mkdir /octopass
WORKDIR /octopass
