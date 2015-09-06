CC=gcc
LIB_FOLDERS=lib/
INC_FOLDERS=include/

CFLAGS=-Wall -I$(INC_FOLDERS) -DGLEW_STATIC
LDFLAGS=-L$(LIB_FOLDERS) -lglfw3 -lopengl32 -lglu32 -lgdi32

ROCHE=bin/roche.exe

roche:$(ROCHE)

bin/roche.exe: obj/main.o obj/glew.o
	$(CC) -o $@ $^ $(LDFLAGS)

obj/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean: 
	rm -rf obj/*.o