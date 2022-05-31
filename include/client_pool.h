#include "csapp.h"

#define MAX_NAME_LEN 21
#define MAX_MESSAGE_LEN 1024

typedef struct { /* represents a pool of connected descriptors */ //line:conc:echoservers:beginpool
    int maxfd;        /* largest descriptor in read_set */   
    fd_set read_set;  /* set of all active descriptors */
    fd_set ready_set; /* subset of descriptors ready for reading  */
    int nready;       /* number of ready descriptors from select */   
    int maxi;         /* highwater index into client array */
    int clientfd[FD_SETSIZE];    /* set of active descriptors */
    char client_name[FD_SETSIZE][MAX_NAME_LEN];
} pool; //line:conc:echoservers:endpool
/* $end echoserversmain */
void init_pool(int httpfd, int wsfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
