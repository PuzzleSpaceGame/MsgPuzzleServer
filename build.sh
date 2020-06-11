#!/bin/bash
mkdir -p build
if [! -d "./rabbitmq-c/build"]; then
    (
        cd rabbitmq-c
        mkdir build
        cd build
        cmake -DENABLE_SSL_SUPPORT=OFF ..
        cmake --build . --target install
    )
for puzzle in puzzle_specific/*; do
    puzzlename=${puzzle##*/}
    (
        cd build
        mkdir -p $puzzlename
    )
    cp puzzle_specific/$puzzlename/* build/$puzzlename
    cp common_original/* build/$puzzlename
    cp common_msg/* build/$puzzlename
    cp common_mine/* build/$puzzlename
    (
        cd build/$puzzlename
        gcc *.c -lrabbitmq -lglibc
    )
done

