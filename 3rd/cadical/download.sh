#! /bin/bash

set -e

mydir=$(dirname $0)

if [ ! -f cadical-3.0.0.tar.gz ]; then
  curl -Ls https://github.com/arminbiere/cadical/archive/refs/tags/rel-3.0.0.tar.gz > cadical-3.0.0.tar.gz
fi
rm -rf cadical-rel-3.0.0
tar xf cadical-3.0.0.tar.gz
