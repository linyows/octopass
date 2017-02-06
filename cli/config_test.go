package main

import "testing"

func TestNewConfig(t *testing.T) {
	opt := &Options{
		Config:  "",
		Version: false,
	}

	c := NewConfig(opt)

	if c.Endpoint != "" {
		t.Errorf("Endpoint expected %s, got %s.", "", c.Endpoint)
	}

	if c.Token != "" {
		t.Errorf("Token expected %s, got %s.", "", c.Token)
	}

	if c.Organization != "" {
		t.Errorf("Organization expected %s, got %s.", "", c.Organization)
	}

	if c.Team != "" {
		t.Errorf("Team expected %s, got %s.", "", c.Team)
	}

	if c.Syslog != false {
		t.Errorf("Syslog expected %s, got %s.", false, c.Syslog)
	}

	if c.MembershipCheck != false {
		t.Errorf("MembershipCheck expected %s, got %s.", false, c.MembershipCheck)
	}
}

func TestNewConfigWithConf(t *testing.T) {
	opt := &Options{
		Config:  "testdata/octopass.conf",
		Version: false,
	}

	c := NewConfig(opt)

	if c.Endpoint != "https://api.ghe-b-domain.com" {
		t.Errorf("Endpoint expected %s, got %s.", "https://api.ghe-b-domain.com", c.Endpoint)
	}

	if c.Token != "b1234567890abcdef" {
		t.Errorf("Token expected %s, got %s.", "b1234567890abcdef", c.Token)
	}

	if c.Organization != "hoge" {
		t.Errorf("Organization expected %s, got %s.", "hoge", c.Organization)
	}

	if c.Team != "fuga" {
		t.Errorf("Team expected %s, got %s.", "fuga", c.Team)
	}

	if c.Syslog != false {
		t.Errorf("Syslog expected %s, got %s.", false, c.Syslog)
	}

	if c.MembershipCheck != false {
		t.Errorf("MembershipCheck expected %s, got %s.", false, c.MembershipCheck)
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
