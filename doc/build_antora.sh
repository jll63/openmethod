#!/bin/bash

#
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/openmethod
#

set -e


if [ $# -eq 0 ]
  then
    echo "No playbook supplied, using default playbook"
    PLAYBOOK="local-playbook.yml"
  else
    PLAYBOOK=$1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

if [ -n "${CIRCLE_REPOSITORY_URL:-}" ]; then
  account="${CIRCLE_REPOSITORY_URL#*:}"
  account="${account%%/*}"
  lib=$(basename "$(git rev-parse --show-toplevel)")
  repository="${account}/$lib"
  sha=${CIRCLE_SHA1}
elif [ -n "${GITHUB_REPOSITORY:-}" ]; then
  repository="${GITHUB_REPOSITORY}"
  sha=${GITHUB_SHA}
fi

if [ -n "${repository}" ] && [ -n "${sha}" ]; then
  base_url="https://github.com/${repository}/blob/${sha}"
  echo "Setting base-url to $base_url"
  cp mrdocs.yml mrdocs.yml.bak
  perl -i -pe 's{^\s*base-url:.*$}{base-url: '"$base_url/"'}' mrdocs.yml
fi

echo "Building documentation with Antora..."
echo "Installing npm dependencies..."
npm ci

echo "Building docs in custom dir..."
PATH="$(pwd)/node_modules/.bin:${PATH}"
export PATH
npx antora --clean --fetch "$PLAYBOOK" --stacktrace --log-level all

echo "Fixing links to non-mrdocs URIs..."

for f in $(find html -name '*.html'); do
  perl -i -pe 's{&lcub;&lcub;(.*?)&rcub;&rcub;}{<a href="../../../$1.html">$1</a>}g' "$f"
done

if [ -n "${base_url:-}" ]; then
  if [ -f mrdocs.yml.bak ]; then
    mv -f mrdocs.yml.bak mrdocs.yml
    echo "Restored original mrdocs.yml"
  else
    echo "mrdocs.yml.bak not found; skipping restore"
  fi
  perl -i -pe "s[{{BASE_URL}}][$base_url]g" \
    html/openmethod/ref_headers.html html/openmethod/BOOST_OPENMETHOD*.html
fi

echo "Done"
