(
	cd ./puzzle_specific
	mkdir -p $1
)
cp ./puzzles/$1.* ./puzzle_specific/$1
./build.sh $1
