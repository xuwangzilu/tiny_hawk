#include "client_pool.h"
#include "websocket.h"

void init_pool(int httpfd, int wsfd, pool *p) 
{
    /* Initially, there are no connected descriptors */
    int i;
    p->maxi = -1;                   //line:conc:echoservers:beginempty
    for (i=0; i< FD_SETSIZE; i++) {  
	p->clientfd[i] = -1;        //line:conc:echoservers:endempty
	p->client_name[i][0] = '\0';
    }

    /* Initially, listenfd is only member of select read set */
    p->maxfd = wsfd;            //line:conc:echoservers:begininit
    FD_ZERO(&p->read_set);
    FD_SET(httpfd, &p->read_set); //line:conc:echoservers:endinit
    FD_SET(wsfd, &p->read_set); //line:conc:echoservers:endinit
}
/* $end init_pool */

void send_names(int connfd, pool* p)
{
    char buf[MAX_MESSAGE_LEN] = "";
    for(int i = p->maxi; i >= 0; --i)
    	if(p->client_name[i][0] != '\0')
    	{
    	    strcat(buf, " ");
    	    strcat(buf, p->client_name[i]);
    	}
    strcat(buf, " name");
    ws_write(connfd, buf, strlen(buf));
}

/* $begin add_client */
void add_client(int connfd, pool *p) 
{
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
	if (p->clientfd[i] < 0) { 
	    /* Add connected descriptor to the pool */
	    p->clientfd[i] = connfd;
	    FD_SET(connfd, &p->read_set);
	    
	    shakehands(connfd);
	    send_names(connfd, p);
	    /* Update max descriptor and pool highwater mark */
	    if (connfd > p->maxfd) //line:conc:echoservers:beginmaxfd
		p->maxfd = connfd; //line:conc:echoservers:endmaxfd
	    if (i > p->maxi)       //line:conc:echoservers:beginmaxi
		p->maxi = i;       //line:conc:echoservers:endmaxi
	    break;
	}
    if (i == FD_SETSIZE) /* Couldn't find an empty slot */
	app_error("add_client error: Too many clients");
}
/* $end add_client */

void broadcast(pool *p, int broadcaster, char* buf, int len)
{
    for (int i = 0;i <= p->maxi; i++)
    	if((i != broadcaster) && (p->clientfd[i] > 0))
    	    ws_write(p->clientfd[i], buf, len);
}

/* $begin check_clients */
void check_clients(pool *p) 
{
    int i, connfd, n;
    char buf[MAX_MESSAGE_LEN]; 
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
	connfd = p->clientfd[i];

	/* If the descriptor is ready, echo a text line from it */
	if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) { 
	    p->nready--;
	    if ((n = ws_read(connfd, buf, MAX_MESSAGE_LEN)) < 0) {
	    	Close(connfd); //line:conc:echoservers:closeconnfd
		FD_CLR(connfd, &p->read_set); //line:conc:echoservers:beginremove
		p->clientfd[i] = -1;          //line:conc:echoservers:endremove
		sprintf(buf, "%s leave", p->client_name[i]);
		broadcast(p, connfd, buf, strlen(buf));
		p->client_name[i][0] = '\0';
	    }
	    else {
	        buf[n++] = '\0';
		if(p->client_name[i][0] == '\0')
		{
		    memcpy(p->client_name[i], buf, n);
		    sprintf(buf, "%s enter", p->client_name[i]);
		    broadcast(p, i, buf, strlen(buf));
		}
		else
		{
		    strcat(buf, " ");
		    strcat(buf, p->client_name[i]);
		    strcat(buf, " message");
		    broadcast(p, i, buf, strlen(buf));
		}
	    }
	}
    }
}
