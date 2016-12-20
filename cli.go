package main

import (
	"fmt"
	"io"
	"os"

	flag "github.com/linyows/mflag"
)

// Exit codes are int values that represent an exit code for a particular error.
const (
	ExitCodeOK    int = 0
	ExitCodeError int = 1
)

// Options is structure
type Options struct {
	Config   string
	Endpoint string
	Token    string
	Belongs  string
	Syslog   bool
	Version  bool
}

// CLI is the command line object
type CLI struct {
	outStream, errStream io.Writer
	inStream             *os.File
}

var usageText = `Usage: octopass [options] <command> [args]

Commands:
  keys   get public keys for AuthorizedKeysCommand in sshd(8)
  pam    authenticate with github for pam_exec(8)

Options:`

var exampleText = `
Examples:
  $ octopass -t <token> keys <user@github>
  $ echo <token@github> | env PAM_USER=<user@github> octopass -t <token> pam

`

// Run invokes the CLI with the given arguments.
func (cli *CLI) Run(args []string) int {
	f := flag.NewFlagSet(Name, flag.ContinueOnError)
	f.SetOutput(cli.outStream)

	f.Usage = func() {
		fmt.Fprintf(cli.outStream, usageText)
		f.PrintDefaults()
		fmt.Fprint(cli.outStream, exampleText)
	}

	var opt Options

	f.StringVar(&opt.Config, []string{"c", "-config"}, "", "the path to the configuration file")
	f.StringVar(&opt.Endpoint, []string{"e", "-endpoint"}, "", "specify github api endpoint")
	f.StringVar(&opt.Token, []string{"t", "-token"}, "", "github personal token for using API")
	f.StringVar(&opt.Belongs, []string{"b", "-belongs"}, "", "organization/team on github")
	f.BoolVar(&opt.Syslog, []string{"s", "-syslog"}, false, "use syslog for log output")
	f.BoolVar(&opt.Version, []string{"v", "-version"}, false, "print the version and exit")

	if err := f.Parse(args[1:]); err != nil {
		return ExitCodeError
	}
	parsedArgs := f.Args()

	if opt.Version {
		fmt.Fprintf(cli.outStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}

	if len(parsedArgs) == 0 {
		f.Usage()
		return ExitCodeOK
	}

	if parsedArgs[0] != "keys" && parsedArgs[0] != "pam" {
		fmt.Fprintf(cli.errStream, "invalid argument: %s\n", parsedArgs[0])
		return ExitCodeError
	}

	c := NewConfig(&opt)
	oct := NewOctopass(c, cli, nil)
	if err := oct.Run(parsedArgs); err != nil {
		fmt.Fprintf(cli.errStream, "%s\n", err)
		return ExitCodeError
	}

	return ExitCodeOK
}
