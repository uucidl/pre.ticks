#!/usr/bin/env sh
HERE="$(dirname ${0})"
BUILD="${HERE}/builds"
[ -d "${BUILD}" ] || mkdir -p "${BUILD}"

"${HERE}"/modules/uu.micros/build --src-dir "${HERE}/src" --output-dir "${BUILD}" "$@"
