package main

import (
	"errors"
	"net/url"
	"strings"

	"github.com/google/go-github/github"
	"golang.org/x/oauth2"
)

// Octopass is struct
type Octopass struct {
	Config *config
	Client *github.Client
}

// DefaultClient returns default Octopass
func DefaultClient(token string, endpoint *url.URL) *github.Client {
	ts := oauth2.StaticTokenSource(
		&oauth2.Token{AccessToken: token},
	)
	tc := oauth2.NewClient(oauth2.NoContext, ts)

	client := github.NewClient(tc)

	if endpoint != nil {
		client.BaseURL = endpoint
	}

	return client
}

// NewOctopass returns Octopass instance
func NewOctopass(c *config, client *github.Client) *Octopass {
	if client == nil {
		client = DefaultClient(c.Token, c.Endpoint)
	}

	return &Octopass{
		Config: c,
		Client: client,
	}
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

// GetUserKeys return public keys
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

// IsBasicAuthorized returns bool
func (o *Octopass) IsBasicAuthorized(u string, pw string) (bool, error) {
	tp := github.BasicAuthTransport{
		Username: strings.TrimSpace(u),
		Password: strings.TrimSpace(pw),
	}
	client := github.NewClient(tp.Client())
	user, _, err := client.Users.Get("")

	if err != nil || u != *user.Login {
		return false, err
	}
	return true, nil
}
