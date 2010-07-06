#ifndef COLOUR_H
#define COLOUR_H

enum colour_t
{
	COLOUR_BLACK,
	COLOUR_NAVY,
	COLOUR_GREEN,
	COLOUR_TEAL,
	COLOUR_MAROON,
	COLOUR_PURPLE,
	COLOUR_GOLD,
	COLOUR_SILVER,
	COLOUR_GREY,
	COLOUR_BLUE,
	COLOUR_LIME,
	COLOUR_AQUA,
	COLOUR_RED,
	COLOUR_PINK,
	COLOUR_YELLOW,
	COLOUR_WHITE
};

enum colour_t str_to_colour(const char *s);
const char *colour_to_str(enum colour_t c);

#endif /* COLOUR_H */
