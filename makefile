clean:
	rm -rf bin/main

doomc:
	clear
	cc src/main_doom.c -o bin/main \
  		-I/opt/homebrew/include/SDL2 \
  		-L/opt/homebrew/lib \
  		-lSDL2
	./bin/main

doomcpp:
	clear
	g++ src/main_doom.cpp -o bin/main \
		-std=c++17 \
		-I. \
		-I/opt/homebrew/include/SDL2 \
		-L/opt/homebrew/lib \
		-lSDL2 -lSDL2_image
	./bin/main

new_doom:
	clear
	cc src/new/*.c -o bin/main \
		-I/opt/homebrew/include/SDL2 \
		-L/opt/homebrew/lib \
		-lSDL2
	./bin/main

1:
	clear
	cc src/1/1_2_doom.c -o bin/main \
		-I/opt/homebrew/cellar/sdl12-compact/1.2.68/include/SDL \
		-L/opt/homebrew/lib \
		-lSDL
	./bin/main