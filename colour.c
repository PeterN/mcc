#include <strings.h>
#include "colour.h"

static const char *s_colours[] = {
	"black",
	"navy",
	"green",
	"teal",
	"maroon",
	"purple",
	"gold",
	"silver",
	"grey",
	"blue",
	"lime",
	"aqua",
	"red",
	"pink",
	"yellow",
	"white",
};

enum colour_t str_to_colour(const char *s)
{
	int i;
	for (i = 0; i <= WHITE; i++) {
		if (!strcasecmp(s, s_colours[i])) return i;
	}
	
	return -1;
}

const char *colour_to_str(enum colour_t c)
{
	return s_colours[c];
}
