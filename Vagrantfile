# -*- mode: ruby -*-
# vi: set ft=ruby :

unless ENV['GITHUB_TOKEN']
  puts 'env GITHUB_TOKEN required'
  exit
end

Vagrant.configure(2) do |config|
  config.vm.synced_folder '.', '/octopass'

  cmd_ubuntu = <<-CMD
    apt-get -yy update
    apt-get install -yy glibc-source gcc make libcurl4-gnutls-dev libjansson-dev vim valgrind
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
    sed -ie 's/GRUB_CMDLINE_LINUX="\(.*\)"/GRUB_CMDLINE_LINUX="net.ifnames=0 biosdevname=0 \1"/g' /etc/default/grub
    update-grub
  CMD

  config.vm.define :ubuntu do |c|
    c.vm.box = 'ubuntu/bionic64'
    c.vm.provision 'shell', inline: cmd_ubuntu + cmd
  end

  config.vm.define :centos do |c|
    c.vm.box = 'centos/7'
    c.vm.provision 'shell', inline: cmd_centos + cmd
  end
end
