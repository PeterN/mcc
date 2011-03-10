#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "util.h"
#include "mcc.h"

int make_path(char *filename)
{
	char *p = filename;
	while (1) {
		/* Get next / */
		p = strchr(p, '/');
		if (p == NULL) return 0;

		/* Terminate and create directory */
		*p = '\0';
		int r = mkdir(filename, S_IRWXU | S_IRWXG | S_IRWXO);
		*p++ = '/';
		if (r != 0 && errno != EEXIST) {
			LOG("make_path on '%s' failed: %s\n", filename, strerror(errno));
			return r;
		}
	}
}

int hash_filename(const char *filename, char *buf, size_t buflen)
{
	char *tmp = strdup(filename);

	/* Split prefix from filename */
	char *prefix, *name;
	char *s = strrchr(tmp, '/');
	if (s == NULL) {
		prefix = ".";
		name   = tmp;
	} else {
		prefix = tmp;
		name   = s + 1;
		*s = '\0';
	}

	/* Get length of name before period */
	size_t len;
	s = strchr(name, '.');
	if (s == NULL) {
		len = strlen(name);
	} else {
		len = s - name;
	}

	int r;
	switch (len) {
		case 1:  r = snprintf(buf, buflen, "%s/_/%c/%s", prefix, name[0], name); break;
		case 2:  r = snprintf(buf, buflen, "%s/_/%c/%c/%s", prefix, name[0], name[1], name); break;
		default: r = snprintf(buf, buflen, "%s/_/%c/%c/%c/%s", prefix, name[0], name[1], name[2], name); break;
	}

	free(tmp);

	lcase(buf);

	return 0;;
}

