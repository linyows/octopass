octopass
========

A authentication command by golang on CLI.

[![Travis](https://img.shields.io/travis/linyows/octopass.svg?style=flat-square)][travis]
[![GitHub release](http://img.shields.io/github/release/linyows/octopass.svg?style=flat-square)][release]
[![MIT License](http://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)][license]
[![Go Documentation](http://img.shields.io/badge/go-documentation-blue.svg?style=flat-square)][godocs]

[travis]: https://travis-ci.org/linyows/octopass
[release]: https://github.com/linyows/octopass/releases
[license]: https://github.com/linyows/octopass/blob/master/LICENSE
[godocs]: http://godoc.org/github.com/linyows/octopass

Description
-----------

This is authenticated with the team on your organization in Github.

Usage
-----

- PAM: pam_exec.so
- SSHD: AuthorizedKeysCommand

Install
-------

To install, use `go get`:

```bash
$ go get -d github.com/linyows/octopass
```

Contribution
------------

1. Fork ([https://github.com/linyows/octopass/fork](https://github.com/linyows/octopass/fork))
1. Create a feature branch
1. Commit your changes
1. Rebase your local changes against the master branch
1. Run test suite with the `go test ./...` command and confirm that it passes
1. Run `gofmt -s`
1. Create a new Pull Request

Author
------

[linyows](https://github.com/linyows)
