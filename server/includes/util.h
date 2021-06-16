#if !defined(_UTIL_H)
#define _UTIL_H


#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

#if !defined(BUFSIZE)
#define BUFSIZE 2256
#endif
#define READ 0
#define WRITE 1

#if !defined(EXTRA_LEN_PRINT_ERROR)
#define EXTRA_LEN_PRINT_ERROR   512
#endif

#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror("\033[1;31m"#name"\033[1;37m");				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define SYSCALL_PRINT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror("\033[1;31m"#name"\033[1;37m");				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;			\
    }

#define SYSCALL_RETURN(name, r, sc, ret, str, ...)	\
    if ((r=sc) == -1) {				\
	perror("\033[1;31m"#name"\033[1;37m");				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;			\
	return ret;                               \
    }


#define SYSCALL_RETURN_FREE(name, r, sc, ret, to_free, str, ...)	\
    if ((r=sc) == -1) {				\
	perror("\033[1;31m"#name"\033[1;37m");				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;		\
  free(to_free);  \
	return ret;                               \
    }

#define CHECK_EQ_EXIT(name, X, val, str, ...)	\
    if ((X)==val) {				\
        perror("\033[1;31m"#name"\033[1;37m");				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define CHECK_EQ_RETURN(name, X, val, ret, str, ...)	\
    if ((X)==val) {				\
        perror("\033[1;31m"#name"\033[1;37m");				\
	print_error(str, __VA_ARGS__);		\
	return ret;			\
  }


#define CHECK_NEQ_EXIT(name, X, val, str, ...)	\
    if ((X)!=val) {				\
        perror("\033[1;31m"#name"\033[1;37m");				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }


/**
 * \brief Procedura di utilita' per la stampa degli errori
 *
 */
static inline void print_error(const char * str, ...) {
    const char err[]="\033[1;31mERROR/WARNING: \033[1;37m";
    va_list argp;
    char * p=(char *)malloc(strlen(str)+strlen(err)+EXTRA_LEN_PRINT_ERROR);
    if (!p) {
	perror("malloc");
        fprintf(stderr,"FATAL ERROR nella funzione 'print_error'\n");
        return;
    }
    strcpy(p,err);
    strcpy(p+strlen(err), str);
    va_start(argp, str);
    vfprintf(stderr, p, argp);
    va_end(argp);
    fprintf(stderr, "\033[0;37m");
    free(p);
}

static inline void print_success(const char * str, ...) {
    const char err[]="\033[1;32mSUCCESS: \033[1;37m";
    va_list argp;
    char * p=(char *)malloc(strlen(str)+strlen(err)+EXTRA_LEN_PRINT_ERROR);
    if (!p) {
	perror("malloc");
        fprintf(stderr,"FATAL ERROR nella funzione 'print_error'\n");
        return;
    }
    strcpy(p,err);
    strcpy(p+strlen(err), str);
    va_start(argp, str);
    vfprintf(stderr, p, argp);
    va_end(argp);
    fprintf(stderr, "\033[0;37m");
    free(p);
}



/** 
 * \brief Controlla se la stringa passata come primo argomento e' un numero.
 * \return  0 ok  1 non e' un numbero   2 overflow/underflow
 */
static inline int isNumber(const char* s, long* n) {
  if (s==NULL) return 1;
  if (strlen(s)==0) return 1;
  char* e = NULL;
  errno=0;
  long val = strtol(s, &e, 10);
  if (errno == ERANGE) return 2;    // overflow/underflow
  if (e != NULL && *e == (char)0) {
    *n = val;
    return 0;   // successo 
  }
  return 1;   // non e' un numero
}

static inline int msleep(long miliseconds)
{
   struct timespec req, rem;

   if(miliseconds > 999)
   {   
        req.tv_sec = (int)(miliseconds / 1000);                            /* Must be Non-Negative */
        req.tv_nsec = (miliseconds - ((long)req.tv_sec * 1000)) * 1000000; /* Must be in range of 0 to 999999999 */
   }   
   else
   {   
        req.tv_sec = 0;                         /* Must be Non-Negative */
        req.tv_nsec = miliseconds * 1000000;    /* Must be in range of 0 to 999999999 */
   }   

   return nanosleep(&req , &rem);
}



static inline int isFloat(const char* s, float* n) {
  if (s==NULL) return 1;
  if (strlen(s)==0) return 1;
  char* e = NULL;
  errno=0;
  float val = strtof(s, &e);
  if (errno == ERANGE) return 2;    // overflow/underflow
  if (e != NULL && *e == (char)0) {
    *n = val;
    return 0;   // successo 
  }
  return 1;   // non e' un numero
}

#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { \
    fprintf(stderr, "ERRORE FATALE lock\n");		    \
    pthread_exit((void*)EXIT_FAILURE);			    \
  }   
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { \
  fprintf(stderr, "ERRORE FATALE unlock\n");		    \
  pthread_exit((void*)EXIT_FAILURE);				    \
  }
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { \
    fprintf(stderr, "ERRORE FATALE wait\n");		    \
    pthread_exit((void*)EXIT_FAILURE);				    \
}
/* ATTENZIONE: t e' un tempo assoluto! */
#define TWAIT(c,l,t) {							\
    int r=0;								\
    if ((r=pthread_cond_timedwait(c,l,t))!=0 && r!=ETIMEDOUT) {		\
      fprintf(stderr, "ERRORE FATALE timed wait\n");			\
      pthread_exit((void*)EXIT_FAILURE);					\
    }									\
  }
#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       {	\
    fprintf(stderr, "ERRORE FATALE signal\n");			\
    pthread_exit((void*)EXIT_FAILURE);					\
  }
#define BCAST(c)     if (pthread_cond_broadcast(c)!=0)    {		\
    fprintf(stderr, "ERRORE FATALE broadcast\n");			\
    pthread_exit((void*)EXIT_FAILURE);						\
  }
  #define LOCK_IFN_RETURN(l, r)  if (pthread_mutex_lock(l)!=0)        {	\
    fprintf(stderr, "ERRORE FATALE lock\n");				\
    return r;								\
  }   
static inline int TRYLOCK(pthread_mutex_t* l) {
  int r=0;		
  if ((r=pthread_mutex_trylock(l))!=0 && r!=EBUSY) {		    
    fprintf(stderr, "ERRORE FATALE unlock\n");		    
    pthread_exit((void*)EXIT_FAILURE);			    
  }								    
  return r;	
}
#define BCAST_RETURN(c, r)     if (pthread_cond_broadcast(c)!=0)    {		\
    fprintf(stderr, "ERRORE FATALE broadcast\n");			\
    return r;						\
  }
#define UNLOCK_IFN_RETURN(l,r)    if (pthread_mutex_unlock(l)!=0)      {	\
    fprintf(stderr, "ERRORE FATALE unlock\n");				\
    return r;								\
  }

#define LOCK_IFN_GOTO(l, r)  if (pthread_mutex_lock(l)!=0)        {	\
    fprintf(stderr, "ERRORE FATALE lock\n");				\
    goto r;								\
}

#define UNLOCK_IFN_GOTO(l,r)    if (pthread_mutex_unlock(l)!=0)      {	\
    fprintf(stderr, "ERRORE FATALE unlock\n");				\
    goto r;								\
  }

#define SIGNAL_IFN_GOTO(c, r, res)    if ((res = pthread_cond_signal(c))!=0)       {	\
    fprintf(stderr, "ERRORE FATALE signal\n");			\
    goto r;					\
}

#define BCAST_IFN_GOTO(c, r)     if (pthread_cond_broadcast(c)!=0)    {		\
    fprintf(stderr, "ERRORE FATALE broadcast\n");			\
    goto r;						\
  }


static inline int TRYLOCK_TIMEOUT(pthread_mutex_t* l, struct timespec *timeout) {
  int r=0;		
  if ((r=pthread_mutex_timedlock(l, &*timeout))!=0 && r!=EBUSY) {		    
    fprintf(stderr, "ERRORE FATALE unlock\n");		    
    pthread_exit((void*)EXIT_FAILURE);			    
  }								    
  return r;	
}

#endif /* _UTIL_H */
