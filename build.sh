#!/bin/bash
mkdir -p build
(
	cd build
	mkdir -p logic_servers
)
if [ ! -d "./rabbitmq-c/build" ]; then
    (
        git submodule update --init --recursive
    	cd rabbitmq-c
        mkdir build
        cd build
        cmake -DENABLE_SSL_SUPPORT=OFF ..
        cmake --build . --target install
	pip3 install pika
    )
fi
for puzzlename in ${1:-$(ls ./puzzle_specific)}; do
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
        gcc -o "$(echo $puzzlename)_logic_server" *.c $CFLAGS -lrabbitmq -lm `pkg-config --cflags --libs glib-2.0` -g -fsanitize=address
	cp "$(echo $puzzlename)_logic_server" ../logic_servers 
    )
    echo "built $puzzlename"
done

