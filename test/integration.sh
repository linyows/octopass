#!/bin/bash

CLR_PASS="\\033[0;32m"
CLR_FAIL="\\033[0;31m"
CLR_WARN="\\033[0;33m"
CLR_INFO="\\033[0;34m"
CLR_RESET="\\033[0;39m"
ALL_PASSED=0

function pass() {
  echo -e "[${CLR_PASS}PASS${CLR_RESET}] octopass::$(echo $1 | sed -e "s/test_//")"
}

function fail() {
  ALL_PASSED=1
  echo -e "[${CLR_FAIL}FAIL${CLR_RESET}] octopass::$(echo $1 | sed -e "s/test_//")"
  echo -e "${CLR_INFO}Expected${CLR_RESET}:"
  echo -e "$2"
  echo -e "${CLR_WARN}Actual${CLR_RESET}:"
  echo -e "$3"
}

function test_octopass_passwd() {
  actual="$(/usr/bin/octopass passwd linyows)"
  expected="linyows:x:74049:2000:managed by octopass:/home/linyows:/bin/bash"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_octopass_shadow() {
  actual="$(/usr/bin/octopass shadow linyows)"
  expected="linyows:!!:::::::"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_getent_passwd() {
  actual="$(getent passwd linyows)"
  expected="linyows:x:74049:2000:managed by octopass:/home/linyows:/bin/bash"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_getent_shadow() {
  actual="$(getent shadow linyows)"
  expected="linyows:!!:::::::"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_id() {
  actual="$(id linyows)"
  if [ "x$TRAVIS" == "xtrue" ]; then
    expected="uid=74049(linyows) gid=2000(travis) groups=2000(travis)"
  else
    expected="uid=74049(linyows) gid=2000(admin) groups=2000(admin)"
  fi

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_public_keys() {
  actual="$(/usr/bin/octopass linyows | head -1)"
  expected="ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCbBkU87QyUEmecsQjCcMTdS6iARCUXzMo2awb4c+irGPUvkXxQUljmLFRXCIw+cEKajiS7VY5NLCZ6WVCbd4yIK+3jdNzrf74isiG8GdU+m64gNGaXtKGFaQEXBp9uWqqZgSw+bVMX2ArOtoh3lP96WJQOoXsOuX0izNS5qf1Z9E01J6IpE3xfudpaL4/vY1RnljM+KUoiIPFqS1Q7kJ+8LpHvV1T9SRiocpLThXOzifzwwoo9I6emwHr+kGwODERYWYvkMEwFyOh8fKAcTdt8huUz8n6k59V9y5hZWDuxP/zhnArUMwWHiiS1C5im8baX8jxSW6RoHuetBxSUn5vR"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_public_keys_user_not_on_github() {
  actual="$(/usr/bin/octopass linyowsfoo | head -1)"
  expected=""

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_shared_user_public_keys() {
  sed -i -e 's/^#SharedUser/SharedUser/g' /etc/octopass.conf

  actual="$(/usr/bin/octopass admin | wc -l)"
  expected="3"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi

  sed -i -e 's/^SharedUser/#SharedUser/g' /etc/octopass.conf
}

function test_authentication() {
  actual="$(echo $OCTOPASS_TOKEN | /usr/bin/octopass pam linyows; echo $?)"
  expected="0"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_authentication_wrong_token() {
  actual="$(echo "1234567890123456789012345678901234567890" | /usr/bin/octopass pam linyows; echo $?)"
  expected="1"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_authentication_wrong_user() {
  actual="$(echo $OCTOPASS_TOKEN | /usr/bin/octopass pam foobar; echo $?)"
  expected="1"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_member_with_collaborators() {
  export OCTOPASS_OWNER=linyows
  export OCTOPASS_REPOSITORY=octopass
  actual="$(getent passwd linyows)"
  expected="linyows:x:74049:2000:managed by octopass:/home/linyows:/bin/bash"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
  unset OCTOPASS_OWNER
  unset OCTOPASS_REPOSITORY
}

function run_test() {
  self=$(cd $(dirname $0) && pwd)/$(basename $0)
  tests="$(grep "^function test_" $self | sed -E "s/function (.*)\(\) \{/\1/g")"
  for t in $(echo $tests); do
    $t
  done
}

run_test
exit $ALL_PASSED
