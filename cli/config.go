package main

import (
	"io/ioutil"

	"github.com/hashicorp/hcl"
)

// Config is the structure
type config struct {
	Endpoint        string
	Token           string
	Organization    string
	Team            string
	Syslog          bool
	MembershipCheck bool
}

// NewConfig creates config from Options
func NewConfig(opt *Options) *config {

	c := &config{
		Endpoint:        "",
		Token:           "",
		Organization:    "",
		Team:            "",
		Syslog:          false,
		MembershipCheck: false,
	}

	if opt.Config == "" {
		return c
	}

	loadedConfig, err := LoadConfig(opt.Config)
	if err != nil {
		panic(err)
	}

	return loadedConfig.Merge(c)
}

// LoadConfig returns config
func LoadConfig(path string) (*config, error) {
	d, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, err
	}

	obj, err := hcl.Parse(string(d))
	if err != nil {
		return nil, err
	}

	var result config
	if err := hcl.DecodeObject(&result, obj); err != nil {
		return nil, err
	}

	return &result, nil
}

// Merge merges config from base config
func (c *config) Merge(c2 *config) *config {
	if c2.Endpoint != "" {
		c.Endpoint = c2.Endpoint
	}
	if c2.Token != "" {
		c.Token = c2.Token
	}
	if c2.Organization != "" {
		c.Organization = c2.Organization
	}
	if c2.Team != "" {
		c.Team = c2.Team
	}
	if c.Syslog != c2.Syslog {
		c.Syslog = c2.Syslog
	}
	if c.MembershipCheck != c2.MembershipCheck {
		c.MembershipCheck = c2.MembershipCheck
	}

	return c
}
