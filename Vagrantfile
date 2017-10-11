# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|
  config.vm.box = 'linyows/centos-7.1'
  config.vm.network :private_network, ip: '192.168.10.10'
  config.vm.synced_folder '.', '/octopass'

  config.vm.provision 'shell', inline: <<-SHELL
    yum install -y glibc gcc make libcurl-devel jansson-devel git vim valgrind
  SHELL
end
