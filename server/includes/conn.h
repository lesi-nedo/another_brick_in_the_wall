#if !defined(CONN_H)
#define CONN_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#define MAXBACKLOG   32

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, signed long long int size, volatile sig_atomic_t *time_to_quit) {
    signed long long int left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
        if(time_to_quit && *time_to_quit ==1) return -1;
	    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK){ 
            continue;
        }
        if(errno == EPIPE) return 0;
	    return -1;
	}
	if (r == 0) return 0;   // EOF
        left    -= r;
	bufptr  += r;
    }
    return 1;
}
/**
 * @brief: same as redn but does not wait for EAGAIN and EWOULDBOCK
 */
static inline int readn_return(long fd, void *buf, signed long long int size, volatile sig_atomic_t *time_to_quit) {
    signed long long int left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=read((int)fd ,bufptr,left)) == -1) {
        if(time_to_quit && *time_to_quit ==1) return -1;
	    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK){ 
            continue;
        }
        if(errno == EPIPE) return 0;
	    return -1;
	}
	if (r == 0) return 0;   // EOF
        left    -= r;
	bufptr  += r;
    }
    return 1;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, signed long long int size, volatile sig_atomic_t *time_to_quit) {
    signed long long int left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
	if ((r=write((int)fd ,bufptr,left)) == -1) {
        if(time_to_quit && *time_to_quit ==1) return -1;
	    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK){ 
            continue;
        }
        if(errno == EPIPE) return 0;
	    return -1;
	}
	if (r == 0) return 0;
        left    -= r;
	bufptr  += r;
    }
    return 1;
}


#endif /* CONN_H */
