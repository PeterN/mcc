#ifndef CONFIG_H
#define CONFIG_H

void config_init(const char *filename);
void config_deinit(void);

void config_set_string(const char *setting, char *value);
void config_set_int(const char *setting, int value);

bool config_get_string(const char *setting, char **value);
bool config_get_int(const char *setting, int *value);

#endif /* CONFIG_H */
