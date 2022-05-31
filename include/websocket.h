#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "openssl/sha.h"
#include "openssl/pem.h"
#include "openssl/bio.h"
#include "openssl/evp.h"


#define BUFFER_SIZE 1024
#define RESPONSE_HEADER_LEN_MAX 1024
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

typedef struct _frame_head {
    char fin;
    char opcode;
    char mask;
    unsigned long long payload_length;
    char masking_key[4];
} frame_head;

int ws_read(int conn,  char* payload_data, int max_len);
int ws_write(int conn, char* buf, int len);
int shakehands(int cli_fd);
