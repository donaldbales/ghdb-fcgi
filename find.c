#include "fcgi_stdio.h" /* fcgi library; put it first*/

#include <ctype.h>
#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "mysql.h"
#include "ghdb-cgi-bin.h"

/*
gcc find.c -o find.fcgi -I /usr/include -l:libfcgi.so -I /usr/include/mysql -l mysqlclient && chmod 755 find.fcgi 

2026-11-19: sudo pmap -x 268238 | grep -e Address -e total, no memory leaks.
*/

int main(int argc, char const *argv[])
{
	int i = 0;

	char *file_buffer= NULL;
	char *file_name = "/var/www/html/ghdbform.html";
	char *pointer = NULL;
	char *temp_buffer = NULL;

	int n = 0;

	debug = 2; //getenv("GHDB_DEBUG") == NULL ? 0 : atoi(getenv("GHDB_DEBUG"));

	if (debug > 1) dprintf(2, "%s: loaded.\n", argv[0]);

	/*
		Get the HTML template
	*/
	file_buffer = get_asset(file_name);

	if (debug > 1) dprintf(2, "%s: initialized.\n", argv[0]);

	/*
		FCGI response loop.
	*/
	n = FCGI_Accept();
	if (debug > 0) fprintf(stderr, "FCGI_Accept=%d\n", n);
	while (n >= 0)
	{
		temp_buffer = malloc(strlen(file_buffer) + 1);
		if (temp_buffer == NULL) 
		{
			fprintf(stderr, "Failed to allocate %ld bytes of memory for temp_buffer.\n", strlen(file_buffer) + 1);
			exit(EXIT_FAILURE);
		}
		strcpy(temp_buffer, file_buffer);

		//puts(temp_buffer);
		for (i=0; i<NUM_FIELDS; i++)
		{
			pointer = strrep(temp_buffer, fields[i].cb, "");
			free(temp_buffer);
			temp_buffer = pointer;
		}
		pointer = strrep(temp_buffer, "{submit}", "Find");
		free(temp_buffer);
		temp_buffer = pointer;
		pointer = strrep(temp_buffer, "/cgi-bin/action.cgi", "/fcgi/select.fcgi");
		free(temp_buffer);
		temp_buffer = pointer;
		pointer = strrep(temp_buffer, "/cgi-bin/select.cgi", "/fcgi/select.fcgi");
		free(temp_buffer);
		temp_buffer = pointer;

		printf("Content-Type:text/html;charset=iso-8859-1\n\n");
		puts(temp_buffer);
		free(temp_buffer);

		n = FCGI_Accept();
		if (debug > 0) fprintf(stderr, "FCGI_Accept=%d\n", n);
	}

	return 0;
}