TEST?=./...
COMMIT = $$(git describe --always)
NAME = "$(shell awk -F\" '/^const Name/ { print $$2; exit }' version.go)"
VERSION = "$(shell awk -F\" '/^const Version/ { print $$2; exit }' version.go)"

default: test

deps:
	go get -d -t ./...
	go get -u github.com/linyows/mflag
	go get github.com/google/go-github/github
	go get golang.org/x/oauth2
	go get github.com/hashicorp/hcl
	go get github.com/golang/lint/golint
	go get golang.org/x/tools/cmd/cover
	go get github.com/pierrre/gotestcover
	go get github.com/mattn/goveralls

depsdev:
	go get -u github.com/mitchellh/gox
	go get -u github.com/tcnksm/ghr

test: deps
	go test -v $(TEST) $(TESTARGS) -timeout=30s -parallel=4
	go test -race $(TEST) $(TESTARGS)

cover: deps
	gotestcover -v -covermode=count -coverprofile=coverage.out -parallelpackages=4 ./...

bin: depsdev
	@sh -c "'$(CURDIR)/scripts/build.sh' $(NAME)"

dist: bin
	ghr v$(VERSION) pkg

.PHONY: default bin dist test testrace deps
