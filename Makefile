CC=gcc
CXX=g++
LIB_FOLDERS=lib/
INC_FOLDERS=include/

CFLAGS=-Wall -I$(INC_FOLDERS) -DGLEW_STATIC
CXXFLAGS=-Wall -I$(INC_FOLDERS) -DGLEW_STATIC
LDFLAGS=-L$(LIB_FOLDERS) -lglfw3 -lopengl32 -lglu32 -lgdi32

ROCHE=bin/roche.exe

roche:$(ROCHE)

bin/roche.exe: obj/main.o obj/glew.o obj/lodepng.o obj/opengl.o obj/util.o obj/planet.o
	$(CXX) -o $@ $^ $(LDFLAGS)

obj/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

obj/%.o: %.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS)

obj/opengl.o: opengl.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS) -fpermissive

clean: 
	rm -rf obj/*.o