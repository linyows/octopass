<p align="center">
  <img alt="OCTOPASS" src="https://github.com/linyows/octopass/blob/master/misc/octopass-logo.png?raw=true" width="400">
</p>

<p align="center">
  Management linux user and authentication with the organization/team on Github.
</p>

<p align="center">
  <a href="https://travis-ci.org/linyows/octopass" title="travis"><img src="https://img.shields.io/travis/linyows/octopass.svg?style=flat-square"></a>
  <a href="https://github.com/linyows/octopass/releases" title="GitHub release"><img src="http://img.shields.io/github/release/linyows/octopass.svg?style=flat-square"></a>
  <a href="https://github.com/linyows/octopass/blob/master/LICENSE" title="MIT License"><img src="https://img.shields.io/badge/license-GPLv3-blue.svg?style=flat-square"></a>
</p>

Description
-----------

This is linux user management tool with the organization/team on github, and authentication.
Depending on github for user management, there are certain risks,
but features easy handling and ease of operation.

Usage
-----

<img alt="Github Org/Team" src="https://github.com/linyows/octopass/blob/master/misc/github-org-team.png?raw=true" width="500" align="right">

For example, adding "Ken" to a team with github organization ...

Octopass is a valid linux server, Ken will be able to ssh login with the key registered in github.

Wow!?

By octopass name resolution, you can check the id of team members of github organization.

```sh
$ id ken
uid=5458(ken) gid=2000(operators) groups=2000(operators)
```
You can also see a list like `/etc/passwd,shadow,group` by the `octopass`.
For detail `--help`.

```sh
$ octopass passwd
chun-li:x:14301:2000:managed by octopass:/home/chun-li:/bin/bash
dhalsim:x:8875:2000:managed by octopass:/home/dhalsim:/bin/bash
ken:x:5458:2000:managed by octopass:/home/ken:/bin/bash
ryu:x:74049:2000:managed by octopass:/home/ryu:/bin/bash
sagat:x:93011:2000:managed by octopass:/home/sagat:/bin/bash
zangief:x:8305:2000:managed by octopass:/home/zangief:/bin/bash
```

And octopass gets the public key from github for key authentication.

```sh
$ octopass ken
ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAqUJvs1vRgHRMH9dpxYcBBV687njS2YrJ+oeIKvbAbg6yL4QsJMeElcPOlmfWEYsp8vbRLXQCTvv14XJfKmgp8V9es5P/l8r5Came3X1S/muqRMONUTdygCpfyo+BJGIMVKtH8fSsBCWfJJ1EYEesyzxqc2u44yIiczM2b461tRwW+7cHNrQ6bKEY9sRMV0p/zkOdPwle30qQml+AlS1SvbrMiiJLEW75dSSENr5M+P4ciJHYXhsrgLE95+ThFPqbznZYWixxATWEYMLiK6OrSy5aYss4o9mvEBJozyrVdKyKz11zSK2D4Z/JTh8eP+NxAw5otqBmfNx+HhKRH3MhJQ==
```

Why?
----

I did not need functions like ldap, and asked for ease and ease of introduction.
Therefore, the user only considers it as administrator authority.
However, it is very easy to add a newly added user or to remove a user who leaves.

Also, in order to speedily resolve names, Github API responses are file cached.
With this, even if Github is down, it will work if past caches remain.

### Architecture

<p align="center">
  <img alt="Architecture" src="https://github.com/linyows/octopass/blob/master/misc/architecture.png?raw=true" width="600">
</p>

Installation
------------

Packages are provided via [packagecloud](https://packagecloud.io/linyows/octopass).

Available for:

- CentOS 7.x
- Ubuntu Xenial

### Building from Source

Dependency

- glibc
- libcurl
- jansson

```
$ git clone https://github.com/linyows/octopass
$ make && make install
$ mv octopass.conf.example /etc/octopass.conf
```

Configuration
-------------

Edit octopass.conf:

```
$ mv /etc/{octopass.conf.example,octopass.conf}
```

Key          | Description                          | Default
---          | ---                                  | ---
Endpoint     | github endpoint                      | https://api.github.com
Token        | github personal access token         | -
Organization | github organization                  | -
Team         | github team                          | -
Group        | group on linux                       | same as team
Home         | user home                            | /home/%s
Shell        | user shell                           | /bin/bash
UidStarts    | start number of uid                  | 2000
Gid          | gid                                  | 2000
Cache        | github api cache sec                 | 500
Syslog       | use syslog                           | false
SharedUsers  | share auth of specific users on team | []

Generate token from here: https://github.com/settings/tokens/new.
Need: Read org and team membership

### SSHD Configuration

/etc/ssh/sshd_config:

```
AuthorizedKeysCommand /usr/bin/octopass
AuthorizedKeysCommandUser root
UsePAM yes
PasswordAuthentication no
```

### PAM Configuration

#### Ubuntu

/etc/pam.d/sshd:

```
#@include common-auth
auth requisite pam_exec.so quiet expose_authtok /usr/bin/octopass pam
auth optional pam_unix.so not_set_pass use_first_pass nodelay
session required pam_mkhomedir.so skel=/etc/skel/ umask=0022
```

#### CentOS

/etc/pam.d/system-auth-ac:

```
# auth        sufficient    pam_unix.so nullok try_first_pass
auth requisite pam_exec.so quiet expose_authtok /usr/bin/octopass pam
auth optional pam_unix.so not_set_pass use_first_pass nodelay
```

/etc/pam.d/sshd:

```
session required pam_mkhomedir.so skel=/etc/skel/ umask=0022
```

### NSS Switch Configuration

/etc/nsswitch.conf:

```
passwd:     files octopass sss
shadow:     files octopass sss
group:      files octopass sss
```

Enable octopass as name resolution.

Author
------

[linyows](https://github.com/linyows)
