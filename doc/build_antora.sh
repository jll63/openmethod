#!/bin/bash

#
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official REPOSITORY: https://github.com/boostorg/openmethod
#

set -ex

if [ $# -eq 0 ]
  then
    echo "No playbook supplied, using default playbook"
    PLAYBOOK="local-playbook.yml"
  else
    PLAYBOOK=$1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

if [ -z "${BOOST_SRC_DIR:-}" ]; then
  CANDIDATE=$( cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd )
  if [ -n "$CANDIDATE" ]; then
    BOOST_SRC_DIR_IS_VALID=ON
    for F in "CMakeLists.txt" "Jamroot" "boost-build.jam" "bootstrap.sh" "libs"; do
      if [ ! -e "$CANDIDATE/$F" ]; then
        BOOST_SRC_DIR_IS_VALID=OFF
        break
      fi
    done
    if [ "$BOOST_SRC_DIR_IS_VALID" = "ON" ]; then
      export BOOST_SRC_DIR="$CANDIDATE"
      echo "Using BOOST_SRC_DIR=$BOOST_SRC_DIR"
    fi
  fi
fi

BRANCH=master

if [ -n "${BOOST_SRC_DIR:-}" ]; then
  if [ -n "${CIRCLE_REPOSITORY_URL:-}" ]; then
    if [[ "$CIRCLE_REPOSITORY_URL" =~ boostorg/boost(\.git)?$ ]]; then
      LIB="$(basename "$(dirname "$SCRIPT_DIR")")"
      REPOSITORY="boostorg/${LIB}"
      BRANCH=$(git -C "$BOOST_SRC_DIR" rev-parse --abbrev-ref HEAD)
    else
      ACCOUNT="${CIRCLE_REPOSITORY_URL#*:}"
      ACCOUNT="${ACCOUNT%%/*}"
      LIB=$(basename "$(git rev-parse --show-toplevel)")
      REPOSITORY="${ACCOUNT}/${LIB}"
    fi
    SHA=$(git -C "$BOOST_SRC_DIR/libs" ls-tree HEAD | grep -w openmethod | awk '{print $3}')
  elif [ -n "${GITHUB_REPOSITORY:-}" ]; then
    REPOSITORY="${GITHUB_REPOSITORY}"
    SHA="${GITHUB_SHA}"
  fi
fi

cd "$SCRIPT_DIR"

# MrDocs takes its own base-url - the one behind the "Declared in <header>" link
# on every reference page - from mrdocs.yml, and the Antora extension invokes it
# with a fixed argument list, so there is no way to pass the commit other than
# editing the file. Restore it from an EXIT trap rather than at the end of the
# script: without one, a failed build leaves mrdocs.yml patched, and the next
# run backs up the patched file and loses the original.
restore_mrdocs_yml() {
  if [ -f "$SCRIPT_DIR/mrdocs.yml.bak" ]; then
    mv -f "$SCRIPT_DIR/mrdocs.yml.bak" "$SCRIPT_DIR/mrdocs.yml"
    echo "Restored original mrdocs.yml"
  fi
}

if [ -n "${REPOSITORY}" ] && [ -n "${SHA}" ]; then
  BASE_URL="https://github.com/${REPOSITORY}/blob/${SHA}"
  echo "Setting base-url to $BASE_URL"
  cp mrdocs.yml mrdocs.yml.bak
  trap restore_mrdocs_yml EXIT
  perl -i -pe 's{^\s*base-url:.*$}{base-url: '"$BASE_URL/"'}' mrdocs.yml
else
  echo "REPOSITORY or SHA not set; skipping base-url modification"
fi

echo "Building documentation with Antora..."
echo "Installing npm dependencies..."
npm ci

echo "Building docs in custom dir..."
PATH="$(pwd)/node_modules/.bin:${PATH}"
export PATH

# ref_headers.adoc links each header to its source with `link:{base-url}/...`.
# Point that at the exact commit when we know it; otherwise antora.yml's
# fallback applies. A command-line attribute outranks the one in antora.yml.
ANTORA_ATTRS=()
if [ -n "${BASE_URL:-}" ]; then
  ANTORA_ATTRS+=(--attribute "base-url=$BASE_URL")
fi

npx antora --clean --fetch "$PLAYBOOK" "${ANTORA_ATTRS[@]}" --stacktrace # --log-level all

echo "Fixing links to non-mrdocs URIs..."
echo "BRANCH='${BRANCH:-}'"
echo "BASE_URL='${BASE_URL:-}'"

for f in $(find html -name '*.html'); do
  perl -i -pe "s{<a href=\"motivation.html\">Boost.OpenMethod</a>}{<a href=\"https://www.boost.org/library/${BRANCH}/openmethod/\">Boost.OpenMethod</a>}g" "$f"
done

echo "Done"
