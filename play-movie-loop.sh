#!/usr/bin/env sh
HERE="$(dirname "${0}")"
BUILD="${HERE}/builds"
[ -d "${BUILD}" ] || mkdir -p "${BUILD}"

# TODO(nicolas): parse include-path
THIS_BUILD="${BUILD}"/"$(hostname)"
THIS_EXECUTABLE="${THIS_BUILD}"/main

#CFLAGS=-fsanitize=address -fsanitize=undefined

if (CPATH=/usr/local/include:"${CPATH}" \
     clang++ -std=c++11 -Wall -Wextra -Wshorten-64-to-32 -Werror -isystem "${HERE}"/modules/uu.micros/include -fno-rtti -Wno-deprecated-declarations -I "${HERE}"/modules/uu.micros/libs/glew/include -I "${HERE}"/modules/uu.micros/libs -Wno-padded -Wno-unused-parameter -Wno-conversion -g -Wno-deprecated-declarations -stdlib=libc++ "${HERE}"/src/play-movie-loop/play-movie-loop.cpp -o "${THIS_EXECUTABLE}" -framework OpenGL "${THIS_BUILD}"/obj/darwin_runtime.cpp.o "${THIS_BUILD}"/obj/glew.c.o -framework AudioToolbox -framework CoreAudio -L "${HERE}"/modules/uu.micros/libs/Darwin_x86_64/ -l glfw3 -framework CoreFoundation -framework CoreGraphics -framework IOKit -framework Cocoa -L/usr/local/lib -lavformat -lavutil -lavcodec -lswscale \
     -g -O1 \
     ${CFLAGS})
then
   printf "EXECUTABLE %s\n" "${THIS_EXECUTABLE}"
   "${THIS_BUILD}"/main "$@"
else
    printf "Try using LIBRARY_PATH or CPLUS_INCLUDE_PATH to tell the compiler where to find libraries\n"
fi
