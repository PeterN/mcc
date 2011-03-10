#ifndef UTIL_H
#define UTIL_H

static inline void lcase(char *buf)
{
	char *p;
	for (p = buf; *p != '\0'; p++)
	{
		if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
	}
}

int make_path(char *filename);
int hash_filename(const char *filename, char *buf, size_t buflen);

#endif /* UTIL_H */
