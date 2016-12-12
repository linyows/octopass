go-teamauth
===========

A authentication command by golang on CLI.

[![Travis](https://img.shields.io/travis/linyows/go-teamauth.svg?style=flat-square)][travis]
[![GitHub release](http://img.shields.io/github/release/linyows/go-teamauth.svg?style=flat-square)][release]
[![MIT License](http://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)][license]
[![Go Documentation](http://img.shields.io/badge/go-documentation-blue.svg?style=flat-square)][godocs]

[travis]: https://travis-ci.org/linyows/go-teamauth
[release]: https://github.com/linyows/go-teamauth/releases
[license]: https://github.com/linyows/go-teamauth/blob/master/LICENSE
[godocs]: http://godoc.org/github.com/linyows/go-teamauth

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
$ go get -d github.com/linyows/go-teamauth
```

Contribution
------------

1. Fork ([https://github.com/linyows/go-teamauth/fork](https://github.com/linyows/go-teamauth/fork))
1. Create a feature branch
1. Commit your changes
1. Rebase your local changes against the master branch
1. Run test suite with the `go test ./...` command and confirm that it passes
1. Run `gofmt -s`
1. Create a new Pull Request

Author
------

[linyows](https://github.com/linyows)
