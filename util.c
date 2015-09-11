#include "util.h"

#include <stdlib.h>
#include <stdio.h>

void read_file(const char* filename, char** buffer_ptr)
{
	long length;
	FILE * f = fopen (filename, "rb");

	if (f)
	{
	  fseek (f, 0, SEEK_END);
	  length = ftell (f);
	  fseek (f, 0, SEEK_SET);
	  char *buffer = malloc (length+1);
	  if (buffer)
	  {
	    fread (buffer, 1, length, f);
	    buffer[length] = 0;
	  }
	  fclose (f);
	  *buffer_ptr = buffer;
	}
}