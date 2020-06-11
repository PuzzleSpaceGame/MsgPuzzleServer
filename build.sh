#!/bin/bash
mkdir -p build
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

