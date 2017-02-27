#!/bin/bash

CLR_PASS="\\033[1;32m"
CLR_FAIL="\\033[1;31m"
CLR_WARN="\\033[1;33m"
CLR_INFO="\\033[1;34m"
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
  echo ""
}

function setup() {
  make
  make install
  test -d /usr/lib/x86_64-linux-gnu && ln -sf /usr/lib/libnss_octopass.so.2.0 /usr/lib/x86_64-linux-gnu/libnss_octopass.so.2.0
  mv octopass.conf.example /etc/octopass.conf
  sed -i -e 's/^passwd:.*/passwd: files octopass/g' /etc/nsswitch.conf
  sed -i -e 's/^shadow:.*/shadow: files octopass/g' /etc/nsswitch.conf
  sed -i -e 's/^group:.*/group: files octopass/g' /etc/nsswitch.conf
}

function test_octopass_passwd() {
  cmd="/usr/bin/octopass passwd linyows"
  actual="$($cmd)"
  expected="linyows:x:74049:2000:managed by octopass:/home/linyows:/bin/bash\n"

  if [ $actual == $expected ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_octopass_shadow() {
  cmd="/usr/bin/octopass shadow linyows"
  actual="$($cmd)"
  expected="linyows:!!::::::::::\n"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_getent_passwd() {
  cmd="getent passwd linyows"
  actual="$($cmd)"
  expected="linyows:x:74049:2000:managed by octopass:/home/linyows:/bin/bash\n"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_getent_shadow() {
  cmd="getent shadow linyows"
  actual="$($cmd)"
  expected="linyows:!!::::::::::\n"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_id() {
  cmd="id linyows"
  actual="$($cmd)"
  expected="uid=74049(linyows) gid=2000(operators) groups=2000(operators)\n"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function test_public_keys() {
  cmd="/usr/bin/octopass linyows"
  actual="$($cmd)"
  expected="ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAQEAqUJvs1vKgHRMH1dpxYcBBV687njS2YrJ+oeIKvbAbg6yL4QsJMeElcPOlmfWEYsp8vbRLXQCTvv14XJfKmgp8V9es5P/l8r5Came3X1S/muqRMONUTdygCpfyo+BJGIMVKtH8fSsBCWfJJ1EYEesyzxqc2u44yIiczM2b461tRwW+7cHNrQ6bKEY9sRMV0p/zkOdPwle30qQml+AlS1SvbrMiiJLEW75dSSENr5M+P4ciJHYXhsrgLE95+ThFPqbznZYWixxATWEYMLiK6OrSy5aYss4o9mvEBJozyrVdKyKz11zSK2D4Z/JTh8eP+NxAw5otqBmfNx+HhKRH3MhJQ==\nssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDpfOPDOHf5ZpFLR2dMhK+B3vSMtAlh/HPOQXsolZYmPQW/xGb0U0+rgXVvBEw193q5c236ENdSrk4R2NE/4ipA/awyCYCJG78Llj2SmqPWbuCtv1K06mXwuh6VM3DP1wPGJmWnzf44Eee4NtTvOzMrORdvGtzQAM044h11N24w07vYwlBvW3P+PdxllbBDJv0ns2A1v40Oerh/xLqAN6UpUADv5prPAnpGnVmuhiNHElX96FmY4y1RxWFNyxnb7/wRwp0NnjfTAmJtB9SWJK9UABLfre2HHlX0gBbhj1+LSW+U5jXD8F9BZF4XRtVY3Ep0PnUrdDqjttrYE0mBfsMh\nssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCbBkU87QyUEmecsQjCcMTdS6iARCUXzMo2awb4c+irGPUvkXxQUljmLFRXCIw+cEKajiS7VY5NLCZ6WVCbd4yIK+3jdNzrf74isiG8GdU+m64gNGaXtKGFaQEXBp9uWqqZgSw+bVMX2ArOtoh3lP96WJQOoXsOuX0izNS5qf1Z9E01J6IpE3xfudpaL4/vY1RnljM+KUoiIPFqS1Q7kJ+8LpHvV1T9SRiocpLThXOzifzwwoo9I6emwHr+kGwODERYWYvkMEwFyOh8fKAcTdt8huUz8n6k59V9y5hZWDuxP/zhnArUMwWHiiS1C5im8baX8jxSW6RoHuetBxSUn5vR\n"

  if [ "x$actual" == "x$expected" ]; then
    pass "${FUNCNAME[0]}"
  else
    fail "${FUNCNAME[0]}" "$expected" "$actual"
  fi
}

function run_test() {
  self=$(cd $(dirname $0) && pwd)/$(basename $0)
  tests=$(grep "function test_" $self | sed -E "s/function (.*)\(\) \{/\1/g")
  for t in $(echo $tests); do; $t; done
}

run_test
exit $ALL_PASSED
