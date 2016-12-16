package main

import (
	"errors"
	"fmt"
	"io/ioutil"
	"net/url"
	"os"
	"strings"

	"github.com/google/go-github/github"
	"golang.org/x/oauth2"
)

// Octopass struct.
type Octopass struct {
	Config *config
	Client *github.Client
	CLI    *CLI
}

// DefaultClient returns default Octopass instance.
func DefaultClient(token string, endpoint string) *github.Client {
	ts := oauth2.StaticTokenSource(
		&oauth2.Token{AccessToken: token},
	)
	tc := oauth2.NewClient(oauth2.NoContext, ts)

	client := github.NewClient(tc)

	if endpoint != "" {
		url, _ := url.Parse(endpoint)
		client.BaseURL = url
	}

	return client
}

// NewOctopass returns Octopass instance.
func NewOctopass(c *config, client *github.Client, cli *CLI) *Octopass {
	if client == nil {
		client = DefaultClient(c.Token, c.Endpoint)
	}

	return &Octopass{
		Config: c,
		Client: client,
		CLI:    cli,
	}
}

// Run controls cli command.
func (o *Octopass) Run(args []string) error {

	u := ""

	stdin, err := o.IsStdinGiven()
	if err != nil {
		return err
	}

	if stdin {
		u = os.Getenv("PAM_USER")
		if err := o.AuthWithTokenFromSTDIN(u); err != nil {
			return err
		}

	} else if len(args) > 0 {
		u = args[0]
		if err := o.OutputKeys(u); err != nil {
			return err
		}

	} else {
		return fmt.Errorf("Arguments or STDIN required")
	}

	if o.Config.Organization != "" && o.Config.Team != "" && u != "" {
		res, err := o.IsMember(u)
		if err != nil {
			return err
		}
		if res == false {
			return nil
		}
	}

	return nil
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
func (o *Octopass) AuthWithTokenFromSTDIN(u string) error {
	PWBytes, err := ioutil.ReadAll(o.CLI.inStream)
	if err != nil {
		return err
	}

	pw := strings.TrimSuffix(string(PWBytes), string("\x00"))
	res, err := o.IsBasicAuthorized(u, pw)
	if err != nil {
		return err
	}
	if res == false {
		return nil
	}

	return nil
}

// IsMember checks if a user is a member of the specified team.
func (o *Octopass) IsMember(u string) (bool, error) {
	opt := &github.ListOptions{PerPage: 100}
	teams, _, err := o.Client.Organizations.ListTeams(o.Config.Organization, opt)
	if err != nil {
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
		return false, errors.New("Team is not exists")
	}

	member, _, err := o.Client.Organizations.IsTeamMember(*teamID, u)
	return member, err
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
func (o *Octopass) IsBasicAuthorized(u string, pw string) (bool, error) {
	tp := github.BasicAuthTransport{
		Username: strings.TrimSpace(u),
		Password: strings.TrimSpace(pw),
	}

	client := github.NewClient(tp.Client())

	if o.Config.Endpoint != "" {
		url, _ := url.Parse(o.Config.Endpoint)
		client.BaseURL = url
	}

	user, _, err := client.Users.Get("")

	if err != nil || u != *user.Login {
		return false, err
	}
	return true, nil
}
