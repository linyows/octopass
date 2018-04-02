FROM debian:stretch
MAINTAINER linyows <linyows@gmail.com>

RUN apt-get -qq update && \
    apt-get install -qq glibc-source gcc make libcurl4-gnutls-dev libjansson-dev \
                        bzip2 unzip debhelper dh-make devscripts cdbs clang apt-utils

ENV USER root

RUN mkdir /octopass
WORKDIR /octopass
