#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <poll.h>

int main(int argc, char *argv[])
{ 

    if (argc <= 2)
    {
        printf("too less\n");
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        printf("connect failed\n");
        close(sockfd);
        return 1;
    }

    
    struct pollfd fsd[2];
    fsd[0].fd = 0;
    fsd[0].events = POLLIN;
    fsd[0].revents = 0;
    fsd[1].fd = sockfd;
    fsd[1].events = POLLIN | POLLRDHUP;
    fsd[1].revents = 0;
    char re_buf[64];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);
    while (1)
    {
        ret = poll(fsd, 2, -1);
        if (ret < 0)
        {
            printf("poll failure\n");
            break;
        }

        if (fsd[1].revents & POLLRDHUP)
        {
            printf("server connection closed\n");
            break;

        
        }else if(fsd[1].revents & POLLIN)
        {
            memset(re_buf,'\0',64);
            recv(fsd[1].fd,re_buf,64,0);
            printf("%s\n",re_buf);


        }
        if(fsd[0].revents& POLLIN)

        {
            //将标准输入直接流到socket上
            ret = splice(0,NULL,pipefd[1],NULL,32768,SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipefd[0],NULL,sockfd,NULL ,32768,SPLICE_F_MORE|SPLICE_F_MOVE);

        }
    }

    close(sockfd);
    return 0;


}
