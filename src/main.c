#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("Logs from your program will appear here!\n");

    int server_fd, client_addr_len;
    struct sockaddr_in client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }

    // Allow address reuse
    int reuse = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) < 0) {

        printf("SO_REUSEADDR failed: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(6379),
        .sin_addr = { htonl(INADDR_ANY) },
    };

    if (bind(
            server_fd,
            (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) != 0) {

        printf("Bind failed: %s\n", strerror(errno));
        return 1;
    }

    int connection_backlog = 5;

    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s\n", strerror(errno));
        return 1;
    }

    printf("Waiting for clients to connect...\n");


    // ========================================
    // epoll
    // ========================================

    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {
        printf("epoll_create1 failed: %s\n", strerror(errno));
        close(server_fd);
        return 1;
    }


    // 把 server_fd 放进 epoll
    struct epoll_event event;

    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(
            epoll_fd,
            EPOLL_CTL_ADD,
            server_fd,
            &event) == -1) {

        printf("epoll_ctl failed: %s\n", strerror(errno));
        close(epoll_fd);
        close(server_fd);
        return 1;
    }


    // ========================================
    // Event Loop
    // ========================================

    struct epoll_event events[10];

    const char *response = "+PONG\r\n";

    while (1) {

        int n = epoll_wait(
            epoll_fd,
            events,
            10,
            -1
        );

        if (n == -1) {
            printf("epoll_wait failed: %s\n", strerror(errno));
            break;
        }


        // 处理所有 ready 的 fd
        for (int i = 0; i < n; i++) {

            int fd = events[i].data.fd;


            // ====================================
            // server_fd 有事件
            // → 有新的 client
            // ====================================

            if (fd == server_fd) {

                client_addr_len = sizeof(client_addr);

                int client_fd = accept(
                    server_fd,
                    (struct sockaddr *) &client_addr,
                    &client_addr_len
                );

                if (client_fd == -1) {
                    printf("Accept failed: %s\n",
                           strerror(errno));
                    continue;
                }

                printf("Client connected\n");


                // 把 client_fd 加入 epoll
                struct epoll_event client_event;

                client_event.events = EPOLLIN;
                client_event.data.fd = client_fd;

                if (epoll_ctl(
                        epoll_fd,
                        EPOLL_CTL_ADD,
                        client_fd,
                        &client_event) == -1) {

                    printf("epoll_ctl client failed: %s\n",
                           strerror(errno));

                    close(client_fd);
                }
            }


            // ====================================
            // client_fd 有事件
            // → client 发来了数据
            // ====================================

            else {

                char buffer[1024];

                ssize_t bytes_read = recv(
                    fd,
                    buffer,
                    sizeof(buffer),
                    0
                );

                if (bytes_read <= 0) {

                    // client 断开
                    close(fd);

                    epoll_ctl(
                        epoll_fd,
                        EPOLL_CTL_DEL,
                        fd,
                        NULL
                    );

                    printf("Client disconnected\n");

                } else {

                    // 现在这个 Stage 只测试 PING
                    send(
                        fd,
                        response,
                        strlen(response),
                        0
                    );
                }
            }
        }
    }


    close(epoll_fd);
    close(server_fd);

    return 0;
}