#!/bin/bash

if [ -z "$1" ]; then
    echo "expected a trurl.1 file to be passed in..."
    exit 1
fi

TRURL_1_FILE=$1



ALL_FLAGS="$(sed  -n \
        -e 's/"//g' \
        -e '/\# URL COMPONENTS/q;p' \
        < "${TRURL_1_FILE}" \
    | grep "##" \
    | awk '{printf "%s%s%s%s ", $2, $3, $4, $5}')"


TRURL_COMPONENT_OPTIONS=""
TRURL_STANDALONE_FLAGS=""
TRURL_RANDOM_OPTIONS=""
TRURL_COMPONENT_LIST="$(sed -n \
      -e 's/"//g' \
      -e '1,/\# URL COMPONENTS/ d' \
      -e '/\# JSON output format/q;p' \
      < "${TRURL_1_FILE}" \
    | grep "##" \
    | awk '{printf "\"%s\" ", $2}')"

for flag in $ALL_FLAGS; do
  # these are now TRURL_STANDALONE 
  if echo "$flag" | grep -q "="; then
    TRURL_COMPONENT_OPTIONS+="$(echo "$flag" \
    | awk '{split($0, a, ","); for(i in a) {printf "%s ", a[i]}}' \
    | cut -f1 -d '[' \
    | awk '{printf "\"%s\" ",  $1}')"
  elif echo "$flag" | grep -q "\["; then
    TRURL_RANDOM_OPTIONS+="$(echo "$flag" \
    | awk '{split($0, a, ","); for(i in a) {printf "%s ", a[i]}}' \
    | cut -f1 -d '[' \
    | awk '{printf "\"%s\" ", $1}')"
  else
    TRURL_STANDALONE_FLAGS+="$(echo "$flag" \
    | awk '{split($0, a, ","); for(i in a) {printf "\"%s\" ",  a[i]}}')"
  fi
done

function generate_zsh() {
    sed -e "s/@TRURL_RANDOM_OPTIONS@/${TRURL_RANDOM_OPTIONS}/g" \
      -e "s/@TRURL_STANDALONE_FLAGS@/${TRURL_STANDALONE_FLAGS}/g" \
      -e "s/@TRURL_COMPONENT_OPTIONS@/${TRURL_COMPONENT_OPTIONS}/g" \
      -e "s/@TRURL_COMPONENT_LIST@/${TRURL_COMPONENT_LIST}/g" \
      ./completions/_trurl.zsh.in  > ./completions/_trurl.zsh
}

generate_zsh "$TRURL_RANDOM_OPTIONS"
