SRC=src
BIN=bin

doom:
	clear
	cc src/main_doom.c -o bin/main \
  		-I/opt/homebrew/include/SDL2 \
  		-L/opt/homebrew/lib \
  		-lSDL2
	./bin/main

wolf:
	clear
	cc src/main_wolf.c -o bin/main \
  		-I/opt/homebrew/include/SDL2 \
  		-L/opt/homebrew/lib \
 		-lSDL2
	./bin/main
