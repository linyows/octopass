package main

import (
	"io/ioutil"

	"github.com/hashicorp/hcl"
)

// Config is the structure
type config struct {
	Endpoint     string
	Token        string
	Organization string
	Team         string
	LogLevel     string
	LogFile      string
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

// SetDefault sets default for config
func (c *config) SetDefault() *config {
	if c.LogLevel == "" {
		c.LogLevel = "DEBUG"
	}

	return c
}
