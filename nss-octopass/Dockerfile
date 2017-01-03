FROM centos:latest
MAINTAINER linyows <linyows@gmail.com>

RUN yum install -y gcc make curl-devel jansson-devel nss-devel autoconf libtool git vim

RUN git clone --depth=1 https://github.com/vstakhov/libucl.git && \
    cd libucl && \
    ./autogen.sh && \
    ./configure && \
    make && make install

RUN git clone https://github.com/allanjude/uclcmd.git
