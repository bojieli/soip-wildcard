#include "common.h"

struct pollfd *fds;
int socknum = 1;

static void begin_conn(int newfd){
    ++socknum;
    fds = safe_realloc(fds, socknum * sizeof(struct pollfd));
    fds[socknum-1].fd = newfd;
    fds[socknum-1].events = POLLIN;
}

static void close_conn(int index){
    close(fds[index].fd);
    int i;
    for (i=index; i<socknum; i++){
        fds[i] = fds[i+1];
    }
    --socknum;
}


struct thread_arg {
    int fd;
    char* pattern;
    int skip;
    int count;
	char* orig_query;
};

void init_search_pattern(struct thread_arg* arg){
    char* result = search_pattern(arg->pattern, arg->skip, arg->count);
    free(arg->pattern);
    if (result == NULL || result[0] == '\0') {
        debug("search %s: no result\n", arg->orig_query);
		int query_len = strlen(arg->orig_query);
		arg->orig_query[query_len] = '\n';
        send(arg->fd, arg->orig_query, query_len+1, 0);
    }
    else {
        debug("search %s: result %s\n", arg->orig_query, result);
        int len = strlen(result);
		int query_len = strlen(arg->orig_query);
		char* sendbuf = safe_malloc(query_len + len + 1);
		memcpy(sendbuf, arg->orig_query, query_len);
		memcpy(sendbuf + query_len, result, len);
        sendbuf[query_len + len] = '\n';

        send(arg->fd, sendbuf, query_len+len+1, 0);
		free(sendbuf);
    }
	if (result)
		free(result);
	free(arg->orig_query);
   	free(arg);
}

// pattern should not be freed
static void dispatch_pattern(int fd, char* pattern, int length){
    struct thread_arg* arg;
    arg = safe_malloc(sizeof(struct thread_arg));
    arg->fd = fd;
    arg->orig_query = safe_malloc(length + 1);
    memcpy(arg->orig_query, pattern, length);
    arg->orig_query[length] = '\0';
    arg->pattern = safe_malloc(length + 1);
    sscanf(arg->orig_query, "%d$%d$%s", &arg->skip, &arg->count, arg->pattern);
    
    pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, (void * (*)(void *))&init_search_pattern, (void *)arg);
}

static int msg_loop(int sockfd) {
    fds = safe_malloc(sizeof(struct pollfd));
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    while (1) {
        IF_ERROR(poll(fds, socknum, -1), "poll")

        // accept connection
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in6 client_addr;
            socklen_t sin_size = sizeof(client_addr);
            int newfd;
            IF_ERROR((newfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)), "accept")
#define BUFSIZE 50
			char buf[BUFSIZE];
			inet_ntop(AF_INET6, (void *)&client_addr.sin6_addr, buf, BUFSIZE);
            printf("client: %s\n", buf);
#undef BUFSIZE
            begin_conn(newfd);
        }

        // read from established connections
        int i;
rescan:
        for (i=1; i<socknum; i++) {
            if (fds[i].revents & POLLIN) {
#define BUFSIZE 1024
                char buf[BUFSIZE];
                int readlen = recv(fds[i].fd, buf, BUFSIZE, 0);
#undef BUFSIZE
                if (readlen == -1) {
                    debug("connection %d closed\n", i);
                    close_conn(i);
                    goto rescan; // socknum is changed
                }
                char *p = buf;
                char *pattern = buf;
                while (p < buf + readlen){
                    if (*p == '\n') {
                        dispatch_pattern(fds[i].fd, pattern, p - pattern);
                        ++p;
                        pattern = p;
                    }
                    else
                        ++p;
                }
            }
        }
    }
}

void init_server(int port)
{
    int sockfd;
    IF_ERROR((sockfd = socket(AF_INET6, SOCK_STREAM, 0)), "socket")
    struct sockaddr_in6 myaddr;
	memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin6_family = AF_INET6;
    myaddr.sin6_port = htons(port);
    myaddr.sin6_addr = in6addr_any;
    int yes = 1;
    IF_ERROR(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)), "setsockopt")
    IF_ERROR(bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)), "bind")
    IF_ERROR(listen(sockfd, 10), "listen")
    printf("Listening on IPv6 port %d\n", port);

    msg_loop(sockfd);
}
