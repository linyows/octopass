package main

import "os"

func main() {
	cli := &CLI{outStream: os.Stdout, errStream: os.Stderr, inStream: os.Stdin}
	os.Exit(cli.Run(os.Args))
}
