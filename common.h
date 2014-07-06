#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <ctype.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <limits.h>
#include <fnmatch.h>

#ifndef NR_OPEN
#define NR_OPEN 1024
#endif

#define debug printf

#define bool unsigned char
#define true 1
#define false 0
#define uchar unsigned char
#define uint unsigned int

// fail fast to find bugs earlier
#define __ASSERT(expr,msg) if (!(expr)) { \
    fprintf(stderr, "assertion failed: %s (errno %d)\n", (msg), errno); \
    exit(1); \
}
#define IF_ERROR(expr,msg) __ASSERT(((expr) != -1), msg)
#define ASSERT(expr) __ASSERT((expr), #expr)

#define __safe_alloc_assert(size) \
    if (__p == NULL) { \
        fprintf(stderr, "failed to allocate %ld bytes\n", (size_t)(size)); \
        exit(1); \
    }
#define safe_realloc(orig,size) ({ \
    void *__p = realloc((orig), (size)); \
    __safe_alloc_assert(size) \
    __p; \
})
#define safe_malloc(size) ({ \
    void *__p = malloc(size); \
    __safe_alloc_assert(size) \
    __p; \
})

char* search_pattern(char* pattern, uint skip, uint count);
void init_server(int port);

#endif
