package main

import (
	"fmt"
	"io/ioutil"
	"net/url"

	"github.com/hashicorp/hcl"
)

// Config is the structure
type config struct {
	Endpoint     *url.URL
	Token        string
	Organization string
	Team         string
}

// LoadConfig returns config
func LoadConfig(path string) (*config, error) {
	d, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("Error reading %s: %s", path, err)
	}

	obj, err := hcl.Parse(string(d))
	if err != nil {
		return nil, fmt.Errorf("Error parsing %s: %s", path, err)
	}

	var result config
	if err := hcl.DecodeObject(&result, obj); err != nil {
		return nil, err
	}

	return &result, nil
}
