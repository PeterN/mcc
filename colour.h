#ifndef COLOUR_H
#define COLOUR_H

enum colour_t
{
	BLACK,
	NAVY,
	GREEN,
	TEAL,
	MAROON,
	PURPLE,
	GOLD,
	SILVER,
	GREY,
	BLUE,
	LIME,
	AQUA,
	RED,
	PINK,
	YELLOW,
	WHITE
};

enum colour_t str_to_colour(const char *s);
const char *colour_to_str(enum colour_t c);

#endif /* COLOUR_H */
