#ifndef NETWORK_WORKER_H
#define NETWORK_WORKER_H

typedef void (*network_callback)(int fd, void *data);

void network_worker_init(void);
void network_worker_deinit(void);

void *network_connect(const char *host, int port, network_callback callback, void *data);

#endif /* NETWORK_WORKER_H */
