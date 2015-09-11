CC=gcc
LIB_FOLDERS=lib/
INC_FOLDERS=include/

CFLAGS=-Wall -I$(INC_FOLDERS) -DGLEW_STATIC
LDFLAGS=-L$(LIB_FOLDERS) -lglfw3 -lopengl32 -lglu32 -lgdi32

ROCHE=bin/roche.exe

roche:$(ROCHE)

bin/roche.exe: obj/main.o obj/glew.o obj/vec3.o obj/mat4.o obj/lodepng.o obj/opengl.o obj/util.o
	$(CC) -o $@ $^ $(LDFLAGS)

obj/vec3.o: vec3.h
obj/mat4.o: mat4.h
obj/opengl.o: opengl.h
obj/util.o: util.h

obj/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean: 
	rm -rf obj/*.o