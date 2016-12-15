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
	Config  string
	Debug   bool
	Version bool
}

// CLI is the command line object
type CLI struct {
	outStream, errStream io.Writer
	opt                  Options
}

var usageText = `Usage: octopass [options]

Options:`

var exampleText = `
SSHD AuthorizedKeysCommand:
  It is returns public-keys
  $ octopass <github username>
  With confirm whether it belongs to the team
  $ octopass <github username> --config=/etc/octopass.conf

PAM Exec:
  Authorize by token
  $ echo <github token> | env PAM_USER=<github username> octopass
  With confirm whether it belongs to the team
  $ echo <github token> | env PAM_USER=<github username> octopass --config=/etc/octopass.conf

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
	f.BoolVar(&cli.opt.Debug, []string{"d", "-debug"}, false, "print log for debug")
	f.BoolVar(&cli.opt.Version, []string{"v", "-version"}, false, "print the version and exit")

	if err := f.Parse(args[1:]); err != nil {
		return ExitCodeError
	}

	if cli.opt.Version {
		fmt.Fprintf(cli.outStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}

	res := ExitCodeOK
	if err := cli.Octopass(f.Args()); err != nil {
		cli.stderr("%#v", err)
		res = ExitCodeError
	}

	return res
}

// Octopass returns exit status
func (cli *CLI) Octopass(args []string) error {
	username := ""

	c, err := LoadConfig(cli.opt.Config)
	if err != nil {
		return err
	}
	o := NewOctopass(c, nil)

	stat, err := os.Stdin.Stat()
	if err != nil {
		return err
	}

	if stat.Size() != 0 {
		// for pam
		PWBytes, err := ioutil.ReadAll(os.Stdin)
		if err != nil {
			return err
		}

		cli.stdout("Got password from STDIN")
		pw := strings.TrimSuffix(string(PWBytes), string("\x00"))
		username = os.Getenv("PAM_USER")
		res, err := o.IsBasicAuthorized(username, pw)
		if err != nil {
			return err
		}
		cli.stdout("Username: %s", username)
		cli.stdout("Basic Authentication: %t", res)

		// for sshd
	} else if len(args) > 0 {
		cli.stdout("Arguments: %s", args)
		username = args[0]
		cli.stdout("Username: %s", username)
		keys, err := o.GetUserKeys(username)
		if err != nil {
			return err
		}
		fmt.Fprintln(cli.outStream, strings.Join(keys, "\n"))

	} else {
		cli.stderr("Arguments or STDIN required")
	}

	if c.Organization != "" && c.Team != "" && username != "" {
		res, err := o.IsMember(username)
		if err != nil {
			return err
		}
		cli.stdout("Team Member: %t", res)
	}

	return nil
}

// Stdout
func (cli *CLI) stdout(format string, a ...interface{}) {
	if cli.opt.Debug {
		fmt.Fprintln(cli.outStream, fmt.Sprintf(format, a...))
	}
}

// Stderr
func (cli *CLI) stderr(format string, a ...interface{}) {
	fmt.Fprintln(cli.errStream, fmt.Sprintf(format, a...))
}
