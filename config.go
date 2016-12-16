package main

import (
	"fmt"
	"io/ioutil"

	"github.com/hashicorp/hcl"
)

// Config is the structure
type config struct {
	Endpoint     string
	Token        string
	Organization string
	Team         string
}

// LoadConfig returns config
func LoadConfig(path string) (*config, error) {
	d, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("[ERR] %s", err)
	}

	obj, err := hcl.Parse(string(d))
	if err != nil {
		return nil, fmt.Errorf("[ERR] %s", err)
	}

	var result config
	if err := hcl.DecodeObject(&result, obj); err != nil {
		return nil, err
	}

	return &result, nil
}
