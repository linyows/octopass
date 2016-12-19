package main

import (
	"bytes"
	"fmt"
	"strings"
	"testing"
)

func TestRun_versionFlag(t *testing.T) {
	outStream, errStream := new(bytes.Buffer), new(bytes.Buffer)
	cli := &CLI{outStream: outStream, errStream: errStream}
	args := strings.Split("./octopass --version", " ")

	status := cli.Run(args)
	if status != ExitCodeOK {
		t.Errorf("expected %d to eq %d", status, ExitCodeOK)
	}

	expected := fmt.Sprintf("octopass version %s", Version)
	if !strings.Contains(outStream.String(), expected) {
		t.Errorf("expected %q to eq %q", outStream.String(), expected)
	}
}

func TestRun_helpFlag(t *testing.T) {
	outStream, errStream := new(bytes.Buffer), new(bytes.Buffer)
	cli := &CLI{outStream: outStream, errStream: errStream}
	args := strings.Split("./octopass --help", " ")

	status := cli.Run(args)
	if status != ExitCodeError {
		t.Errorf("expected %d to eq %d", status, ExitCodeError)
	}

	expected := `Usage: octopass [options] <command> [args]

Commands:
  keys   get public keys for AuthorizedKeysCommand in sshd(8)
  pam    authorize with github authentication for pam_exec(8)

Options:
  -b, --belongs      organization/team on github
  -c, --config       the path to the configuration file
  -e, --endpoint     specify github api endpoint
  -s, --syslog       use syslog for log output
  -t, --token        github personal token for using API
  -v, --version      print the version and exit

Examples:
  $ octopass -t <token> keys <user@github>
  $ echo <token@github> | env PAM_USER=<user@github> octopass -t <token> pam

`

	if !strings.Contains(outStream.String(), expected) {
		t.Errorf("expected %q to eq %q", outStream.String(), expected)
	}
}
