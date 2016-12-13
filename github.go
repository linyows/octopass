package main

import (
	"github.com/google/go-github/github"
	"golang.org/x/oauth2"
)

// Client returns github.Client
func Client() (*github.Client, error) {
	c := Conf()
	ts := oauth2.StaticTokenSource(
		&oauth2.Token{AccessToken: c.Token},
	)
	tc := oauth2.NewClient(oauth2.NoContext, ts)

	client := github.NewClient(tc)
	if c.Endpoint != nil {
		client.BaseURL = c.Endpoint
	}

	return client, nil
}

// PublicKeys returns keys
func PublicKeys(username string) ([]string, error) {
	client, _ := Client()
	opt := &github.ListOptions{PerPage: 100}
	githubKeys, _, err := client.Users.ListKeys(username, opt)
	if err != nil {
		return nil, err
	}

	var keys []string
	for _, key := range githubKeys {
		keys = append(keys, *key.Key)
	}

	return keys, nil
}
