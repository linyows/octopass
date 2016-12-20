package main

import (
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/url"
	"os"
	"strings"

	"github.com/google/go-github/github"
	"golang.org/x/oauth2"
)

// Octopass struct.
type Octopass struct {
	Config *config
	CLI    *CLI
	Client *github.Client
}

// NewOctopass returns Octopass instance.
func NewOctopass(c *config, cli *CLI, client *github.Client) *Octopass {
	var logWriter io.Writer
	if c.Syslog == false {
		logWriter = cli.errStream
	}

	if err := SetupLogger(logWriter); err != nil {
		panic(err)
	}

	if client == nil {
		client = DefaultClient(c.Token, c.Endpoint)
	}

	return &Octopass{
		Config: c,
		CLI:    cli,
		Client: client,
	}
}

// DefaultClient returns default Octopass instance.
func DefaultClient(token string, endpoint string) *github.Client {
	ts := oauth2.StaticTokenSource(
		&oauth2.Token{AccessToken: token},
	)
	tc := oauth2.NewClient(oauth2.NoContext, ts)

	client := github.NewClient(tc)

	if endpoint != "" {
		log.Print("[DEBUG] " + fmt.Sprintf("Specified API endpoint: %s", endpoint))
		url, _ := url.Parse(endpoint)
		client.BaseURL = url
	}

	return client
}

// Run controls cli command.
func (o *Octopass) Run(args []string) error {
	var u string

	switch args[0] {
	case "keys":
		if len(args) == 0 {
			return fmt.Errorf("User required")
		}
		u = args[1]
		log.Print("[DEBUG] " + fmt.Sprintf("Keys request user: %s", u))

		if o.isConfirmable() {
			res, err := o.IsMember(u)
			if err != nil {
				return err
			}
			if res == false {
				return errors.New("")
			}
		}

		if err := o.OutputKeys(u); err != nil {
			return err
		}

	case "pam":
		stdin, err := o.IsStdinGiven()
		if err != nil {
			return err
		}
		if stdin == false {
			return fmt.Errorf("STDIN required")
		}
		if u = os.Getenv("PAM_USER"); u == "" {
			return fmt.Errorf("PAM_USER required")
		}
		log.Print("[DEBUG] " + fmt.Sprintf("env PAM_USER: %s", u))

		if o.isConfirmable() {
			res, err := o.IsMember(u)
			if err != nil {
				return err
			}
			if res == false {
				return errors.New("")
			}
		}

		res, err := o.AuthWithTokenFromSTDIN(u)
		if err != nil {
			return err
		}
		if res == false {
			return fmt.Errorf("Not authenticated")
		}
	}

	return nil
}

func (o *Octopass) isConfirmable() bool {
	return o.Config.Organization != "" && o.Config.Team != "" && o.Config.MembershipCheck == true
}

// IsStdinGiven checks given stdin
func (o *Octopass) IsStdinGiven() (bool, error) {
	stat, err := o.CLI.inStream.Stat()
	if err != nil {
		return false, err
	}

	mode := stat.Mode()
	if (mode&os.ModeNamedPipe != 0) || mode.IsRegular() {
		return true, nil
	}

	return false, nil
}

// OutputKeys outputs user public-keys.
func (o *Octopass) OutputKeys(u string) error {
	keys, err := o.GetUserKeys(u)
	if err != nil {
		return err
	}
	fmt.Fprintln(o.CLI.outStream, strings.Join(keys, "\n"))
	return nil
}

// AuthWithTokenFromSTDIN authorize as basic-authentication on Github.
func (o *Octopass) AuthWithTokenFromSTDIN(u string) (bool, error) {
	PWBytes, err := ioutil.ReadAll(o.CLI.inStream)
	if err != nil {
		return false, err
	}

	pw := strings.TrimSuffix(string(PWBytes), string("\x00"))
	res := o.IsBasicAuthorized(u, pw)

	return res, nil
}

// IsMember checks if a user is a member of the specified team.
func (o *Octopass) IsMember(u string) (bool, error) {
	opt := &github.ListOptions{PerPage: 100}
	teams, _, err := o.Client.Organizations.ListTeams(o.Config.Organization, opt)

	if err != nil {
		log.Print("[INFO] " + fmt.Sprintf(
			"Organization \"%s\" team list error: %s", o.Config.Organization, err))
		return false, err
	}

	var teamID *int
	for _, team := range teams {
		if *team.Name == o.Config.Team {
			teamID = team.ID
			break
		}
	}

	if teamID == nil {
		log.Print("[INFO] " + fmt.Sprintf(
			"\"%s/%s\" is not exists on github", o.Config.Organization, o.Config.Team))
		return false, nil
	}

	isMember, _, err := o.Client.Organizations.IsTeamMember(*teamID, u)

	if err != nil {
		return false, err
	}

	if isMember {
		log.Print("[INFO] " + fmt.Sprintf(
			"\"%s/%s\" includes: %s", o.Config.Organization, o.Config.Team, u))
	} else {
		log.Print("[INFO] " + fmt.Sprintf(
			"\"%s/%s\" not includes: %s", o.Config.Organization, o.Config.Team, u))
	}

	return isMember, err
}

// GetUserKeys return public keys.
func (o *Octopass) GetUserKeys(u string) ([]string, error) {
	var keys []string

	opt := &github.ListOptions{PerPage: 100}
	githubKeys, _, err := o.Client.Users.ListKeys(u, opt)
	if err != nil {
		return keys, err
	}

	for _, key := range githubKeys {
		keys = append(keys, *key.Key)
	}
	return keys, nil
}

// IsBasicAuthorized authorize to Github.
func (o *Octopass) IsBasicAuthorized(u string, pw string) bool {
	tp := github.BasicAuthTransport{
		Username: strings.TrimSpace(u),
		Password: strings.TrimSpace(pw),
	}

	client := github.NewClient(tp.Client())

	if o.Config.Endpoint != "" {
		log.Print("[DEBUG] " + fmt.Sprintf("Specified API endpoint as basic auth: %s", o.Config.Endpoint))
		url, _ := url.Parse(o.Config.Endpoint)
		client.BaseURL = url
	}

	_, _, err := client.Users.Get("")

	if err != nil {
		log.Print("[INFO] " + fmt.Sprintf("Failed basic authentication -- %s", err))
		return false
	}

	log.Print("[INFO] " + fmt.Sprintf("Passed basic-authentication user: %s", u))
	return true
}
