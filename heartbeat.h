#ifndef HEARTBEAT_H
#define HEARTBEAT_H

extern int s_heartbeat_fd;

void heartbeat_start();
void heartbeat_run(bool can_write, bool can_read);

#endif /* HEARTBEAT_H */
