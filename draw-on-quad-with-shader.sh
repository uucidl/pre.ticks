#!/usr/bin/env sh
HERE="$(dirname "${0}")"
BUILD="${HERE}/builds"
[ -d "${BUILD}" ] || mkdir -p "${BUILD}"

# we use the basename of this very script here to derive the src
# directory to build.
#
# so when adding a new program at src/program_name/main.cpp, simply
# copying this .sh into a file named program_name.sh will let you
# compile & run it
#
"${HERE}"/scripts/run.sh --top-dir "${HERE}" --src-dir "${HERE}"/src/"$(basename ${0} .sh)" "$@"
