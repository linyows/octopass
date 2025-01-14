#!/bin/sh

# Returns Distribution ID from PackageCloud

check_jq() {
  if ! command -v jq &> /dev/null; then
    echo "jq is not installed" >&2
    exit 1
  fi
}

list() {
  if [ $PACKAGECLOUD_TOKEN == "" ]; then
    echo '$PACKAGECLOUD_TOKEN is required'
    exit 1
  fi

  curl -s -H "Authorization: Bearer $PACKAGECLOUD_TOKEN" \
    https://packagecloud.io/api/v1/distributions.json > distributions.json
}

find() {
  os=$1
  if [ "$os" = "" ]; then
    os=ubuntu
  fi

  code=$2
  if [ "$code" = "" ]; then
    code=noble
  fi

  number=$(cat distributions.json | \
    jq ".deb[] | select(.index_name == \"$os\") | .versions[] | {\"id\":.id,\"name\":.index_name}" | \
    jq -s | jq ".[] | select(.name == \"$code\") | .id")

  printf "$number"
}

#check_jq
list
find $1 $2
rm -rf distributions.json
