FROM ubuntu:precise
MAINTAINER linyows <linyows@gmail.com>

RUN apt-get -qq update && \
    apt-get install -qq libc6 gcc make libcurl4-gnutls-dev libjansson-dev \
                        bzip2 unzip debhelper dh-make devscripts cdbs clang

ENV USER root

RUN mkdir /octopass
WORKDIR /octopass
