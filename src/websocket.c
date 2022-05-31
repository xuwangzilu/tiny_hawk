#include "websocket.h"

unsigned char *base64_encode(unsigned char *str)
{
    long len;
    long str_len;
    unsigned char *res;
    int i,j;

    unsigned char *base64_table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 

    str_len=SHA_DIGEST_LENGTH;//str_len=strlen(str);
    if(str_len % 3 == 0)
        len=str_len/3*4;
    else
        len=(str_len/3+1)*4;
 
    res=malloc(sizeof(unsigned char)*len+1);
    res[len]='\0';
 
    for(i=0,j=0;i<len-2;j+=3,i+=4)
    {
        res[i]=base64_table[str[j]>>2]; 
        res[i+1]=base64_table[(str[j]&0x3)<<4 | (str[j+1]>>4)]; 
        res[i+2]=base64_table[(str[j+1]&0xf)<<2 | (str[j+2]>>6)]; 
        res[i+3]=base64_table[str[j+2]&0x3f]; 
    }
 
    switch(str_len % 3)
    {
        case 1:
            res[i-2]='=';
            res[i-1]='=';
            break;
        case 2:
            res[i-1]='=';
            break;
    }
 
    return res;
}

int _readline(char* allbuf,int level,char* linebuf)
{
    int len = strlen(allbuf);
    for (;level<len;++level)
    {
        if(allbuf[level]=='\r' && allbuf[level+1]=='\n')
            return level+2;
        else
            *(linebuf++) = allbuf[level];
    }
    return -1;
}

int shakehands(int cli_fd)
{
    //next line's point num
    int level = 0;
    //all request data
    char buffer[BUFFER_SIZE];
    //a line data
    char linebuf[256];
    //Sec-WebSocket-Accept
    char sec_accept[32];
    //sha1 data
    unsigned char sha1_data[SHA_DIGEST_LENGTH+1]={0};
    //reponse head buffer
    char head[BUFFER_SIZE] = {0};

    if (read(cli_fd,buffer,sizeof(buffer))<=0)
        perror("read");
    printf("request\n");
    printf("%s\n",buffer);

    do {
        memset(linebuf,0,sizeof(linebuf));
        level = _readline(buffer,level,linebuf);
        //printf("line:%s\n",linebuf);

        if (strstr(linebuf,"Sec-WebSocket-Key")!=NULL)
        {
            strcat(linebuf,GUID);
            
            SHA1((unsigned char*)&linebuf+19,strlen(linebuf+19),(unsigned char*)&sha1_data);

            /* write the response */
            sprintf(head, "HTTP/1.1 101 Switching Protocols\r\n" \
                          "Upgrade: websocket\r\n" \
                          "Connection: Upgrade\r\n" \
                          "Sec-WebSocket-Accept: %s\r\n" \
                          "\r\n",base64_encode(sha1_data));

            printf("response\n");
            printf("%s",head);
            if (write(cli_fd,head,strlen(head))<0)
                perror("write");

            break;
        }
    }while((buffer[level]!='\r' || buffer[level+1]!='\n') && level!=-1);
    return 0;
}

void inverted_string(char *str,int len)
{
    int i; char temp;
    for (i=0;i<len/2;++i)
    {
        temp = *(str+i);
        *(str+i) = *(str+len-i-1);
        *(str+len-i-1) = temp;
    }
}

int recv_frame_head(int fd,frame_head* head)
{
    char one_char;
    /*read fin and op code*/
    if (read(fd,&one_char,1)<=0)
    {
        perror("read fin");
        return -1;
    }
    head->fin = (one_char & 0x80) == 0x80;
    head->opcode = one_char & 0x0F;
    if (read(fd,&one_char,1)<=0)
    {
        perror("read mask");
        return -1;
    }
    head->mask = (one_char & 0x80) == 0X80;

    /*get payload length*/
    head->payload_length = one_char & 0x7F;

    if (head->payload_length == 126)
    {
        char extern_len[2];
        if (read(fd,extern_len,2)<=0)
        {
            perror("read extern_len");
            return -1;
        }
        head->payload_length = (extern_len[0]&0xFF) << 8 | (extern_len[1]&0xFF);
    }
    else if (head->payload_length == 127)
    {
        char extern_len[8];
        if (read(fd,extern_len,8)<=0)
        {
            perror("read extern_len");
            return -1;
        }
        inverted_string(extern_len,8);
        memcpy(&(head->payload_length),extern_len,8);
    }

    /*read masking-key*/
    if (read(fd,head->masking_key,4)<=0)
    {
        perror("read masking-key");
        return -1;
    }

    return 0;
}

void umask(char *data,int len,char *mask)
{
    int i;
    for (i=0;i<len;++i)
        *(data+i) ^= *(mask+(i%4));
}

int send_frame_head(int fd,int payload_length)
{
    char *response_head;
    int head_length = 0;
    if(payload_length<126)
    {
        response_head = (char*)malloc(2);
        response_head[0] = 0x81;
        response_head[1] = payload_length;
        head_length = 2;
    }
    else if (payload_length<0xFFFF)
    {
        response_head = (char*)malloc(4);
        response_head[0] = 0x81;
        response_head[1] = 126;
        response_head[2] = (payload_length >> 8 & 0xFF);
        response_head[3] = (payload_length & 0xFF);
        head_length = 4;
    }
    else
    {
        response_head = (char*)malloc(12);
        response_head[0] = 0x81;
        response_head[1] = 127;
        memcpy(response_head+2,&payload_length,sizeof(unsigned long long));
        inverted_string(response_head+2,sizeof(unsigned long long));
        head_length = 12;
    }

    if(write(fd,response_head,head_length)<=0)
    {
        perror("write head");
        return -1;
    }

    free(response_head);
    return 0;
}

int ws_read(int conn,  char* payload_data, int max_len)
{
    frame_head head;
    if (recv_frame_head(conn,&head))
        return -1;
    if (head.opcode == 8)
        return -1;
    int len = read(conn, payload_data, max_len);
    umask(payload_data,len,head.masking_key);
    return len;
}

int ws_write(int conn, char* buf, int len)
{
    if(send_frame_head(conn, len))
        return -1;
    return write(conn, buf, len);
}




