package main

import "testing"

func TestNewConfig(t *testing.T) {
	opt := &Options{
		Config:   "",
		Endpoint: "https://api.ghe-a-domain.com",
		Token:    "a1234567890abcdef",
		Belongs:  "foo/bar",
		Syslog:   false,
		Version:  false,
	}

	c := NewConfig(opt)

	if c.Endpoint != opt.Endpoint {
		t.Errorf("Endpoint expected %s, got %s.", opt.Endpoint, c.Endpoint)
	}

	if c.Token != opt.Token {
		t.Errorf("Token expected %s, got %s.", opt.Token, c.Token)
	}

	if c.Organization != "foo" {
		t.Errorf("Organization expected %s, got %s.", "foo", c.Organization)
	}

	if c.Team != "bar" {
		t.Errorf("Team expected %s, got %s.", "bar", c.Team)
	}

	if c.Syslog != false {
		t.Errorf("Syslog expected %s, got %s.", false, c.Syslog)
	}

	if c.MembershipCheck != true {
		t.Errorf("MembershipCheck expected %s, got %s.", true, c.MembershipCheck)
	}
}

func TestNewConfigWithConf(t *testing.T) {
	opt := &Options{
		Config:   "testdata/octopass.conf",
		Endpoint: "https://api.ghe-a-domain.com",
		Token:    "a1234567890abcdef",
		Belongs:  "foo/bar",
		Syslog:   false,
		Version:  false,
	}

	c := NewConfig(opt)

	if c.Endpoint != opt.Endpoint {
		t.Errorf("Endpoint expected %s, got %s.", opt.Endpoint, c.Endpoint)
	}

	if c.Token != opt.Token {
		t.Errorf("Token expected %s, got %s.", opt.Token, c.Token)
	}

	if c.Organization != "foo" {
		t.Errorf("Organization expected %s, got %s.", "foo", c.Organization)
	}

	if c.Team != "bar" {
		t.Errorf("Team expected %s, got %s.", "bar", c.Team)
	}

	if c.Syslog != false {
		t.Errorf("Syslog expected %s, got %s.", false, c.Syslog)
	}

	if c.MembershipCheck != true {
		t.Errorf("MembershipCheck expected %s, got %s.", true, c.MembershipCheck)
	}
}

func TestLoadConfig(t *testing.T) {
	c, err := LoadConfig("testdata/octopass.conf")
	if err != nil {
		t.Errorf("Raised error %#v.", err)
	}

	expect := "https://api.ghe-b-domain.com"
	if c.Endpoint != expect {
		t.Errorf("Endpoint expected %s, got %s.", expect, c.Endpoint)
	}
}
