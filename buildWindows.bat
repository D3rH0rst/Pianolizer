@echo off

set SOURCE_DIR=./src/
set BUILD_DIR=./build/

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

set CFLAGS=-Wall -Wextra

set DEPENDENCIES_DIR=./WinDependencies/
set LIB_DIR=%DEPENDENCIES_DIR%bin/
rem gcc %CFLAGS% -o %BUILD_DIR%pianolizer.exe  %SOURCE_DIR%main.c %SOURCE_DIR%plug.c %DEPENDENCIES_DIR%bin/libraylib.a %DEPENDENCIES_DIR%bin/libfluidsynth.dll.a -lopengl32 -lgdi32 -lwinmm

gcc %CFLAGS% -o %BUILD_DIR%pianolizer.exe  %SOURCE_DIR%main.c %SOURCE_DIR%plug.c -L%LIB_DIR% -lraylib -lfluidsynth -lopengl32 -lgdi32 -lwinmm 