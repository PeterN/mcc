#ifndef IRC_H
#define IRC_H

extern int s_irc_fd;

void irc_start();
void irc_run(bool can_write, bool can_read);

#endif /* IRC_H */
