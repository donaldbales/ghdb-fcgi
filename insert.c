#include "fcgi_stdio.h" /* fcgi library; put it first*/

#include <ctype.h>
#include <mysql.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ghdb-cgi-bin.h"

/*
gcc insert.c -o insert.fcgi -I /usr/include -l:libfcgi.so -I /usr/include/mysql -l mysqlclient && chmod 755 insert.fcgi

cat Apple.txt | env CONTENT_LENGTH=`stat -c %s Apple.txt` ./insert.fcgi

2026-11-18: Fixed. sudo pmap -x 262329 | grep -e Address -e total, no memory leaks.
*/

int main(int argc, char const *argv[])
{
	int i;

	int key_values_length = 0;
	struct KEY_VALUE *key_values;

	char **values;
	int field_index;
	int values_index = 0;
	int values_length = 0;
	char *temp = NULL;

	long unsigned lengths[NUM_FIELDS];

	MYSQL *mysql = NULL;
	MYSQL_STMT *stmt = NULL;
	MYSQL_BIND bind[NUM_FIELDS];
	char *bind_buffer;
	long unsigned *bind_length;
	bool *bind_is_null;
	uint64_t affected_rows;

	int n = 0;

	debug = 2; //getenv("GHDB_DEBUG") == NULL ? 0 : atoi(getenv("GHDB_DEBUG"));

	if (debug > 1) dprintf(2, "%s: loaded.\n", argv[0]);

	/*
		Get a connection to MySQL
	*/
	mysql = get_mysql_connection();
	if (mysql == NULL)
	{
		exit(EXIT_FAILURE);
	}

	if (debug > 0) fprintf(stderr, "Initialize a new INSERT prepared statement.\n");
	stmt = mysql_stmt_init(mysql);
	if (mysql_stmt_errno(stmt) != 0)
	{
		fprintf(stderr, "%s\n", mysql_stmt_error(stmt));
		exit(EXIT_FAILURE);
	}
	if (debug > 0) fprintf(stderr, "New prepared statement: %p\n", stmt);

	if (debug > 0) fprintf(stderr, "Prepare the statement.\n");
	i = mysql_stmt_prepare(stmt, ghdb_insert, strlen(ghdb_insert));
	if (mysql_stmt_errno(stmt) != 0)
	{
		fprintf(stderr, "%s\n", mysql_stmt_error(stmt));
		exit(EXIT_FAILURE);
	}

	if (debug > 1) dprintf(2, "%s: initialized.\n", argv[0]);

	/*
		FCGI response loop.
	*/
	n = FCGI_Accept();
	if (debug > 0) fprintf(stderr, "FCGI_Accept=%d\n", n);
	while (n >= 0)
	{
		/*
			Get the POST key value pairs
		*/
		key_values_length = 0;
		values_length = 0;
		key_values = get_post_data(&key_values_length);
		if (debug > 0) fprintf(stderr, "key_values_length=%d\n", key_values_length);
		if (key_values_length > 0)
		{		
			values = (char **)malloc(sizeof(char *) * NUM_FIELDS);
			if (values == NULL)
			{
				fprintf(stderr, "Failed to allocate values memory.\n");
				exit(EXIT_FAILURE);
			}
			for (i = 0; i < NUM_FIELDS; i++)
			{
				*(values + i) = (char *)malloc(sizeof(char) * 2001);
				if (*(values + i) == NULL)
				{
					fprintf(stderr, "Failed to allocate value memory.\n");
					exit(EXIT_FAILURE);
				}
				strcpy(*(values + i), "");
			}
			for (i = 0; i < key_values_length; i++)
			{
				field_index = get_field_index((key_values + i)->key);
				if (debug > 0) fprintf(stderr, "field_index=%d\n", field_index);
				if (debug > 0) fprintf(stderr, "(key_values + i)->value=%s\n", (key_values + i)->value);
				if (debug > 0) 
				{
					temp = url_decode((key_values + i)->value);
					fprintf(stderr, "url_decode((key_values + i)->value)=%s\n", temp);
					free(temp);
				}
				if (field_index > -1)
				{
					values_length++;
					temp = url_decode((key_values + i)->value);
					strcpy(*(values + field_index), temp);
					free(temp);
					if (debug > 0) fprintf(stderr, "*(values + %d)=\"%s\"\n", field_index, *(values + field_index));
				} 
			}
		}
		for (i = 0; i < values_length; i++)
		{
			free((key_values + i)->key);
			free((key_values + i)->value);
		}
		free(key_values);

		if (debug > 0) fprintf(stderr, "Build the bind variable.\n");
		if (debug > 0) fprintf(stderr, "Set the bind array to zeroes.\n");
		for (i = 0; i < NUM_FIELDS; i++)
		{
			if (debug > 0) fprintf(stderr, "Initialize the bind structure for %s.\n", fields[i].n);
			bind_buffer = malloc(strlen(*(values + i)) + 1);
			if (bind_buffer == NULL)
			{
				fprintf(stderr, "Failed to allocate bind_buffer memory.\n");
				exit(EXIT_FAILURE);
			}
			strcpy(bind_buffer, *(values + i));

			bind_length = malloc(sizeof(unsigned long int));
			if (bind_length == NULL)
			{
				fprintf(stderr, "Failed to allocate bind_length memory.\n");
				exit(EXIT_FAILURE);
			}
			*(bind_length) = strlen(*(values + i));
			lengths[i] = *(bind_length);

			bind_is_null = malloc(sizeof(int));
			if (bind_is_null == NULL)
			{
				fprintf(stderr, "Failed to allocate bind_is_null memory.\n");
				exit(EXIT_FAILURE);
			}
			*(bind_is_null) = (*(bind_length) == 0) ? 1 : 0;

			bind[i].buffer_type = fields[i].mst;
			bind[i].buffer = bind_buffer;
			bind[i].length = bind_length;
			bind[i].is_null = bind_is_null;
		}

		if (debug > 0) fprintf(stderr, "Bind the parameters.\n");
		mysql_stmt_bind_param(stmt, bind);
		if (mysql_stmt_errno(stmt) != 0)
		{
			fprintf(stderr, "%s\n", mysql_stmt_error(stmt));
			exit(EXIT_FAILURE);
		}

		if (debug > 0) fprintf(stderr, "Execute the prepared statement.\n");
		i = mysql_stmt_execute(stmt);
		if (debug > 0) fprintf(stderr, "i=%d\n", i);
		if (mysql_stmt_errno(stmt) != 0)
		{
			fprintf(stderr, "%s\n", mysql_stmt_error(stmt));
			exit(EXIT_FAILURE);
		}

		affected_rows = mysql_stmt_affected_rows(stmt);
		if (debug > 0) fprintf(stderr, "%ld row inserted.\n", affected_rows);

		printf("Status: 303 See other\n");
		printf("Location: /fcgi/add.fcgi\n\n");
		fflush(stdout);

		/*
			Clean up the bind buffer memory
		*/
		for (i = 0; i < NUM_FIELDS; i++)
		{
			free(bind[i].buffer);
			free(bind[i].length);
			free(bind[i].is_null);
		}
		for (i = 0; i < NUM_FIELDS; i++)
		{
			free(*(values + i));
		}
		free(values);

		n = FCGI_Accept();
		if (debug > 0) fprintf(stderr, "FCGI_Accept=%d\n", n);
	} // while

	if (debug > 0) fprintf(stderr, "Close the prepared statement:%p\n", stmt);
	mysql_stmt_close(stmt);
	if (debug > 0) fprintf(stderr, "Close the mysql connection:%p\n", mysql);
	mysql_close(mysql);
	fflush(stderr);

	return 0;
}