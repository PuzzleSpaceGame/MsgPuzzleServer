#!/bin/bash
mkdir -p build
if [ ! -d "./rabbitmq-c/build" ]; then
    (
        git submodule update --init --recursive
    	cd rabbitmq-c
        mkdir build
        cd build
        cmake -DENABLE_SSL_SUPPORT=OFF ..
        cmake --build . --target install
    )
fi
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
        gcc *.c -lrabbitmq -lm `pkg-config --cflags --libs glib-2.0`
    )
done

