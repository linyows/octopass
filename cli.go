package main

import (
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"strings"

	flag "github.com/linyows/mflag"
)

// Exit codes are int values that represent an exit code for a particular error.
const (
	ExitCodeOK    int = 0
	ExitCodeError int = 1 + iota
)

// Options is structure
type Options struct {
	Belongs  string
	Endpoint string
	Debug    bool
	Version  bool
}

// CLI is the command line object
type CLI struct {
	outStream, errStream io.Writer
	ops                  Options
}

var usageText = `Usage: octopass [options]

Options:`

var exampleText = `
SSHD AuthorizedKeysCommand:
  It is returns public-keys
  $ octopass <github username>
  With confirm whether it belongs to the team
  $ octopass <github username> --belongs=foo/bar

PAM Exec:
  Authorize by token
  $ echo <github token> | env PAM_USER=<github username> octopass
  With confirm whether it belongs to the team
  $ echo <github token> | env PAM_USER=<github username> octopass --belongs=foo/bar

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

	f.StringVar(&cli.ops.Belongs, []string{"b", "-belongs"}, "", "organization/team on github")
	f.StringVar(&cli.ops.Endpoint, []string{"e", "-endpoint"}, "", "github api endpoint")
	f.BoolVar(&cli.ops.Debug, []string{"d", "-debug"}, false, "print log for debug")
	f.BoolVar(&cli.ops.Version, []string{"v", "-version"}, false, "print the version and exit")

	if err := f.Parse(args[1:]); err != nil {
		f.Usage()
		return ExitCodeError
	}

	if cli.ops.Version {
		fmt.Fprintf(cli.outStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}

	res := ExitCodeOK
	if err := cli.Octopass(f.Args()); err != nil {
		res = ExitCodeError
	}

	return res
}

// Octopass returns exit status
func (cli *CLI) Octopass(args []string) error {
	if cli.ops.Belongs != "" {
		belongs := strings.Split(cli.ops.Belongs, "/")
		org := belongs[0]
		team := belongs[1]
		cli.stdout("Github org/team: %s/%s", org, team)
	}
	if cli.ops.Endpoint != "" {
		c := Conf()
		c.Set(cli.ops.Endpoint)
	}

	// for sshd
	if len(args) > 0 {
		cli.stdout("Arguments: %s", args)
		username := args[0]
		cli.stdout("Username: %s", username)
		PublicKeys(username)

		// for pam
	} else {
		PWBytes, err := ioutil.ReadAll(os.Stdin)
		if err != nil {
			return err
		}

		cli.stdout("Got password from STDIN")
		pw := strings.TrimSuffix(string(PWBytes), string("\x00"))
		cli.stdout("PW: %s", pw)
	}

	return nil
}

// Stdout
func (cli *CLI) stdout(format string, a ...interface{}) {
	if cli.ops.Debug {
		fmt.Fprintln(cli.outStream, fmt.Sprintf(format, a...))
	}
}

// Stderr
func (cli *CLI) stderr(format string, a ...interface{}) {
	fmt.Fprintln(cli.errStream, fmt.Sprintf(format, a...))
}
