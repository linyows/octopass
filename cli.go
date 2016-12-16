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
	Config  string
	Version bool
}

// CLI is the command line object
type CLI struct {
	outStream, errStream io.Writer
	inStream             *os.File
	opt                  Options
}

var usageText = `Usage: octopass [options] <github username>

Options:`

var exampleText = `
SSHD AuthorizedKeysCommand:
  $ octopass <github username>
PAM Exec:
  $ echo <github token> | env PAM_USER=<github username> octopass

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

	f.StringVar(&cli.opt.Config, []string{"c", "-config"}, "/etc/octopass.conf", "the path to the configuration file")
	f.BoolVar(&cli.opt.Version, []string{"v", "-version"}, false, "print the version and exit")

	if err := f.Parse(args[1:]); err != nil {
		return ExitCodeError
	}

	if cli.opt.Version {
		fmt.Fprintf(cli.outStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}

	c, err := LoadConfig(cli.opt.Config)
	if err != nil {
		fmt.Fprintf(cli.errStream, "%s\n", err)
		return ExitCodeError
	}

	oct := NewOctopass(c, nil, cli)
	if err := oct.Run(f.Args()); err != nil {
		fmt.Fprintf(cli.errStream, "%s\n", err)
		return ExitCodeError
	}

	return ExitCodeOK
}
