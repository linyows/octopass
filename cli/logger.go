package main

import (
	"io"
	"log"
	"log/syslog"
)

// SetupLogger sets syslog if writer is nil
func SetupLogger(w io.Writer) error {
	if w == nil {
		l, err := syslog.New(syslog.LOG_NOTICE|syslog.LOG_USER, Name)
		if err != nil {
			return err
		}
		log.SetOutput(l)

	} else {
		log.SetOutput(w)
	}

	return nil
}
