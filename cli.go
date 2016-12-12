package main

import (
	"fmt"
	"io"

	"github.com/jessevdk/go-flags"
	flag "github.com/linyows/mflag"
)

// Exit codes are int values that represent an exit code for a particular error.
const (
	ExitCodeOK    int = 0
	ExitCodeError int = 1 + iota
)

// CLI is the command line object
type CLI struct {
	outStream, errStream io.Writer
	ops                  Ops
}

// Ops is structure
type Ops struct {
	Config       string
	Organization string
	Team         string
	Verbose      bool
	Version      bool
}

var usageText = `
Usage: teamauth [options]
Options:`

var exampleText = `
Example:
  $ teamauth <github username> --organization github --team foo
  $ echo <github token> | teamauth --config=/etc/teamauth.conf
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

	var ops Ops
	f.StringVar(&ops.Config, []string{"c", "-config"}, "", "config path")
	f.StringVar(&ops.Organization, []string{"o", "-organization"}, "", "organization on github")
	f.StringVar(&ops.Team, []string{"t", "-team"}, "", "team on organization")
	f.BoolVar(&ops.Verbose, []string{"l", "-verbose"}, false, "print verbose log")
	f.BoolVar(&ops.Version, []string{"v", "-version"}, false, "print the version and exit")

	if err := flags.Parse(args[1:]); err != nil {
		return ExitCodeError
	}

	if ops.Version {
		fmt.Fprintf(cli.errStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}

	return cli.TeamAuth(ops)
}

// Stdout
func (cli *CLI) out(format string, a ...interface{}) {
	if cli.ops.Verbose {
		fmt.Fprintln(cli.outStream, fmt.Sprintf(format, a...))
	}
}

// Stderr
func (cli *CLI) err(format string, a ...interface{}) {
	fmt.Fprintln(cli.errStream, fmt.Sprintf(format, a...))
}
