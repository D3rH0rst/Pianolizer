@echo off

REM Define the source and build directories
set SOURCE_DIR=./src/
set BUILD_DIR=./build/

REM Create the build directory if it doesn't exist
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM Set compiler flags and libraries
set CFLAGS=-Wall -Wextra

REM Set the path to your WinDependencies folder (replace with your actual path)
set DEPENDENCIES_DIR=WinDependencies

REM Build the project
gcc %CFLAGS% -o %BUILD_DIR%pianolizer.exe  %SOURCE_DIR%main.c %SOURCE_DIR%plug.c %SOURCE_DIR%parsemidi.c %SOURCE_DIR%midi-parser.c %DEPENDENCIES_DIR%/bin/libraylib.a -lopengl32 -lgdi32 -lwinmm
