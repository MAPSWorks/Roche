CC=gcc
LIB_FOLDERS=lib/
INC_FOLDERS=include/

CFLAGS=-Wall -I$(INC_FOLDERS) -DGLEW_STATIC
LDFLAGS=-L$(LIB_FOLDERS) -lglfw3 -lopengl32 -lglu32 -lgdi32

ROCHE=bin/roche.exe

roche:$(ROCHE)

bin/roche.exe: obj/main.o obj/glew.o obj/lodepng.o obj/opengl.o obj/util.o obj/vecmath.o
	$(CC) -o $@ $^ $(LDFLAGS)

obj/opengl.o: opengl.h
obj/util.o: util.h
obj/vecmath.o: vecmath.h

obj/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean: 
	rm -rf obj/*.o