#!/bin/sh


set -xe

DEBUG="-ggdb"

CFLAGS="-Wall -Wextra $DEBUG `pkg-config --cflags raylib` `pkg-config --cflags fluidsynth`"
LIBS="`pkg-config --libs raylib` `pkg-config --libs fluidsynth`"

mkdir -p ./build

#build the hot reload DLL
clang $CFLAGS -o ./build/libplug.so -fPIC -shared ./src/plug.c $LIBS

# build with hot reload enabled
clang $CFLAGS -DHOTRELOAD -o ./build/pianolizer ./src/hotreload.c ./src/main.c $LIBS

#build with hot reload disabled (link at compile time)
#clang $CFLAGS -o ./build/pianolizer ./src/plug.c ./src/main.c $LIBS
