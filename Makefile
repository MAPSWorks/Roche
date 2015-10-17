CC=gcc
CXX=g++
LIB_FOLDERS=lib/
INC_FOLDERS=include/

CFLAGS=-Wall -I$(INC_FOLDERS) -DGLEW_STATIC
CXXFLAGS=-Wall -I$(INC_FOLDERS) -DGLEW_STATIC -std=c++11 -pthread
LDFLAGS=-L$(LIB_FOLDERS) -lglfw3 -lopengl32 -lglu32 -lgdi32 -static-libgcc -static-libstdc++ -pthread

ROCHE=bin/roche.exe

roche:$(ROCHE)

bin/roche.exe: obj/main.o obj/glew.o obj/opengl.o obj/util.o obj/planet.o obj/game.o obj/shaun.o obj/sweeper.o
	$(CXX) -o $@ $^ $(LDFLAGS)

obj/game.o: game.h opengl.h util.h
obj/main.o: game.h
obj/opengl.o: opengl.h util.h
obj/planet.o: planet.h opengl.h util.h
obj/util.o: util.h

obj/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

obj/%.o: %.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)

obj/opengl.o: opengl.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS) -fpermissive

obj/%.o : shaun/%.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)

clean: 
	rm -rf obj/*.o