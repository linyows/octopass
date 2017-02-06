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
}

var usageText = `Usage: octopass [options] [args]

Options:`

var exampleText = `
Examples:
  $ octopass <user@github>
  $ echo <token@github> | env PAM_USER=<user@github> octopass

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

	f.StringVar(&opt.Config, []string{"c", "-config"}, "/etc/octopass.conf", "the path to the configuration file")
	f.BoolVar(&opt.Version, []string{"v", "-version"}, false, "print the version and exit")

	if err := f.Parse(args[1:]); err != nil {
		return ExitCodeError
	}
	parsedArgs := f.Args()

	if opt.Version {
		fmt.Fprintf(cli.outStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}

	c := NewConfig(&opt)
	oct := NewOctopass(c, cli, nil)
	if err := oct.Run(parsedArgs); err != nil {
		msg := fmt.Sprintf("%s", err)
		if msg != "" {
			fmt.Fprintf(cli.errStream, "%s\n", err)
		}
		return ExitCodeError
	}

	return ExitCodeOK
}
