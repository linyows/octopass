# -*- mode: ruby -*-
# vi: set ft=ruby :

unless ENV['GITHUB_TOKEN']
  puts 'env GITHUB_TOKEN required'
  exit
end

Vagrant.configure(2) do |config|
  cmd_ubuntu = <<-CMD
    apt-get -yy update
    apt-get install -yy glibc-source gcc make libcurl4-gnutls-dev libjansson-dev vim valgrind systemd-coredump
    timedatectl set-timezone Asia/Tokyo
  CMD

  cmd_centos = <<-CMD
    yum install -y glibc gcc make libcurl-devel jansson-devel git vim valgrind
  CMD

  cmd = <<-CMD
    echo 'root:root' | chpasswd
    cd /octopass && make build && make install
    cp /octopass/misc/octopass.conf /etc/octopass.conf
    cp /octopass/misc/nsswitch.conf /etc/nsswitch.conf
    sed -i 's/GITHUB_TOKEN/#{ENV['GITHUB_TOKEN']}/' /etc/octopass.conf
    ulimit -c unlimited

    # selinux policy
    make selinux_policy
    semodule -i /octopass/selinux/octopass.pp

    # sshd
    cat << EOS >> /etc/ssh/sshd_config
AuthorizedKeysCommand /usr/bin/octopass
AuthorizedKeysCommandUser root
UsePAM yes
PasswordAuthentication no
EOS

    # pam
    cp /etc/pam.d/sshd /tmp/pam.d-sshd
    cat << EOS > /etc/pam.d/sshd
auth requisite pam_exec.so quiet expose_authtok /usr/bin/octopass pam
auth optional pam_unix.so not_set_pass use_first_pass nodelay
session required pam_mkhomedir.so skel=/etc/skel/ umask=0022
EOS
    cat /tmp/pam.d-sshd >> /etc/pam.d/sshd
  CMD

  config.vm.define :ubuntu do |c|
    c.vm.box = 'ubuntu/bionic64'
    c.vm.provision 'shell', inline: cmd_ubuntu + cmd
    c.vm.synced_folder '.', '/octopass'
  end

  config.vm.define :centos do |c|
    c.vagrant.plugins = 'vagrant-vbguest'
    c.vm.box = 'centos/7'
    c.vm.provision 'shell', inline: cmd_centos + cmd
    c.vm.synced_folder '.', '/octopass'
  end
end
