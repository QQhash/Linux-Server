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
#include <errno.h>
#define user_limit 10
#define BUFFER 64
#define fd_limit 65535
struct client_data
{
    struct sockaddr_in addrsss;
    char *write_buf;
    char buf[BUFFER];
};
int setnoblocking(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}
int main(int argc, char *argv[])
{
    if (argc <= 2)
    {

        printf("too less\n");
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);
    struct client_data *users = (struct client_data *)malloc(sizeof(struct client_data) * fd_limit);
    struct pollfd fds[user_limit + 1];
    int user_counter = 0;
    for (int i = 0; i <= user_limit; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = listenfd;             //文件描述符
    fds[0].events = POLLIN | POLLERR; //注册的事件
    //POLLIN数据可读
    //POLLERR错误
    fds[0].revents = 0; //内核自己的事件由内核填充
    while (1)
    {
        ret = poll(fds, user_counter + 1, -1);
        if (ret < 0)
        {
            printf("poll failure");
            break;
        }
        for (int i = 0; i < user_counter + 1; ++i)
        {
            if ((fds[i].fd == listenfd) && (fds[i].revents & POLLIN)) //第一有数据可以连接
            {
                struct sockaddr_in client_adress;
                socklen_t client_addrlen = sizeof(client_adress);
                int connfd = accept(listenfd, (struct sockaddr *)&client_adress, &client_addrlen);
                if (connfd <= 0)
                {
                    printf("error is%d\n", errno);
                    continue;
                }
                if (user_counter >= user_limit)
                {
                    const char *info = "too much users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    continue;
                }
                user_counter++;
                users[connfd].addrsss = client_adress;
                setnoblocking(connfd);
                fds[user_counter].fd = connfd;//用户描述符
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;//注册用户事件
                fds[user_counter].revents = 0;
                printf("come a new user,now have %d users\n", user_counter);
            }
            else if (fds[i].revents & POLLERR) //第二有数据错误
            {
                printf("get an error from%d\n", fds[i].fd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0)
                {
                    printf("get opt failed\n");
                }
                continue;
            }
            else if (fds[i].revents & POLLRDHUP) //第三tcp被关闭
            {
                users[fds[i].fd] = users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i] = fds[user_counter];
                i--;
                user_counter--;
                printf("a client left\n");
            }
            else if (fds[i].revents & POLLIN) //第四有数据可以读
            {
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER);
                ret = recv(connfd, users[connfd].buf, BUFFER, 0);
                printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);
                if (ret < 0)
                {
                    if (errno != EAGAIN)
                    {
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if (ret = 0)
                {
                    printf("read  0 bytes\n");
                }
                else
                {
                    for (int j = 1; j <= user_counter; ++j)
                    {
                        if (fds[j].fd == connfd)
                        {
                            continue;
                        }
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            }
            else if (fds[i].revents & POLLOUT) //第五数据可写
            {
                int connfd = fds[i].fd;
                if (!users[connfd].write_buf)
                {
                    continue;
                }
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                fds[i].events |= ~POLLIN;
                fds[i].events |= POLLOUT;
            }
        }
    }
    free(users);

    close(listenfd);
    return 0;
}
