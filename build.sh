#!/bin/bash

rm -r build
mkdir build
pushd build

cmake -DBUILD_DOC=TRUE ..
cmake --build .
