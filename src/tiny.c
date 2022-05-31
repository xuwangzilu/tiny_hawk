#include "csapp.h"
#include "client_pool.h"

void handleHttp(int fd);

int main(int argc, char **argv) 
{
    int httpfd, wsfd, connfd;
    char port[8];
    socklen_t clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_storage clientaddr;
    static pool pool; 

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    sprintf(port, "%d", atoi(argv[1]) + 1);
    printf("%s\n", port);
    httpfd = Open_listenfd(argv[1]);
    wsfd = Open_listenfd(port);
    init_pool(httpfd, wsfd, &pool);
    while(1)
    {
    	pool.ready_set = pool.read_set;
	pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

	if (FD_ISSET(httpfd, &pool.ready_set)) { 
	    pool.nready--;
	    connfd = Accept(httpfd, (SA *)&clientaddr, &clientlen); 
	    handleHttp(connfd);                                     
	    Close(connfd);
	}
	
	if (FD_ISSET(wsfd, &pool.ready_set)) { 
	    connfd = Accept(wsfd, (SA *)&clientaddr, &clientlen);
	    add_client(connfd, &pool);
	}
	
	check_clients(&pool); 
    }
    return 0;
}
