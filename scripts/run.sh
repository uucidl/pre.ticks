#!/usr/bin/env sh
SCRIPTS="$(dirname "${0}")"

TOP="${SCRIPTS}"/..

while [ "$#" -gt 0 ]; do
    case $1 in
        --top-dir)
            TOP=${2:?"source directory expected"}
            if [ ! -d "${TOP}" ]; then
                printf -- "top dir expected in '%s'\n" "${TOP}"
                exit 1
            fi
            
            shift
            shift
            ;;
        *)
            break
            ;;
    esac
done

BUILD="${TOP}"/builds
[ -d "${BUILD}" ] || mkdir -p "${BUILD}"

"${SCRIPTS}"/build.sh --top-dir "${TOP}" "$@" && "${BUILD}"/"$(hostname)"/main
