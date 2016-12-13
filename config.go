package main

import (
	"net/url"
	"os"
)

// Config is the structure
type Config struct {
	Token    string
	Endpoint *url.URL
}

var configInstance *Config

// Conf returns Config singleton structure.
func Conf() *Config {
	if configInstance == nil {
		configInstance = &Config{
			Token:    os.Getenv("GITHUB_TOKEN"),
			Endpoint: nil,
		}
	}

	return configInstance
}

// Set sets endpoint
func (c *Config) Set(endpoint string) *Config {
	url, _ := url.Parse(endpoint)
	c.Endpoint = url
	return c
}
