FROM centos:7
MAINTAINER linyows <linyows@gmail.com>

RUN yum install -y glibc gcc make libcurl-devel jansson-devel bzip2 unzip rpmdevtools mock epel-release selinux-policy-devel
# For epel
RUN yum install -y clang

RUN mkdir -p /root/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
RUN sed -i "s;%_build_name_fmt.*;%_build_name_fmt\t%%{ARCH}/%%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.el7.rpm;" /usr/lib/rpm/macros

RUN mkdir /octopass
WORKDIR /octopass
