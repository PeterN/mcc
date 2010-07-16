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

#define TAG_BLACK "&0"
#define TAG_NAVY "&1"
#define TAG_GREEN "&2"
#define TAG_TEAL "&3"
#define TAG_MAROON "&4"
#define TAG_PURPLE "&5"
#define TAG_GOLD "&6"
#define TAG_SILVER "&7"
#define TAG_GREY "&8"
#define TAG_BLUE "&9"
#define TAG_LIME "&a"
#define TAG_AQUA "&b"
#define TAG_RED "&c"
#define TAG_PINK "&d"
#define TAG_YELLOW "&e"
#define TAG_WHITE "&f"

enum colour_t str_to_colour(const char *s);
const char *colour_to_str(enum colour_t c);

#endif /* COLOUR_H */
