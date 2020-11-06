// MATTEO STEFANELLI MAT: 543781
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <pthread.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <signal.h>

#define Len_Sockname 14

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int running = 1;
 int pipeaddr, k;

static void* Client_Manager(void*  args);
static void gestore(int num);
static void thread_cleanup_handler(void);

int main(int argc, char* argv[]){
  if( argc!=3 ){
    fprintf( stderr, "SERVER: argc diverso da 3\n" );
    exit(EXIT_FAILURE);
  }
  k = atoi(argv[1]);
  pipeaddr = atoi( argv[2] );

  struct sigaction sigus, sigint;
  memset(&sigus, 0, sizeof(sigus));
  sigus.sa_handler = gestore;
  sigaction(SIGUSR1, &sigus, NULL);
  memset(&sigint, 0, sizeof(sigint));
  sigint.sa_handler = SIG_IGN;
  sigaction(SIGINT, &sigint, NULL);

  printf( "SERVER %d ACTIVE\n", k );
  fflush(stdout);

  int fd_skt;
  struct sockaddr_un sa;
  char sockname[Len_Sockname];
  snprintf( sockname, Len_Sockname, "OOB-server-%d", k );
  unlink( sockname );
  strncpy( sa.sun_path, sockname, sizeof(char) * Len_Sockname );
  sa.sun_family = AF_UNIX;
  fd_skt = socket( AF_UNIX, SOCK_STREAM, 0 );
  bind( fd_skt, ( struct sockaddr* ) &sa, sizeof ( sa ) );
  pthread_t tid;
  listen(fd_skt,SOMAXCONN);
  int addrlen = sizeof(sa);
  while( running ){
      int* fdc = malloc(sizeof(int));
      *fdc = accept(fd_skt, ( struct sockaddr* ) &sa, (socklen_t*) &addrlen);
      if(*fdc>=0){
        printf( "SERVER %d CONNECT FROM CLIENT\n", k );
        fflush(stdout);
        pthread_create( &tid, NULL, &Client_Manager, fdc );
        pthread_detach(tid);
      }
  }

  unlink( sockname );
  exit( EXIT_SUCCESS );
}

static void *Client_Manager( void* args ){

  int fdc = *((int*)args);
  free(args);
  struct timespec times[2], tm;
  uint64_t netid, hostid = -1;
  int i = 0, delta = 0, secret = 0;
  atexit(thread_cleanup_handler);
  while( read( fdc, &netid, 8 ) >0 && running){
    clock_gettime( CLOCK_MONOTONIC, &tm );
    if( i%2 == 0 ){
      times[0].tv_sec = tm.tv_sec;
      times[0].tv_nsec = tm.tv_nsec;
    }else{
      times[1].tv_sec = tm.tv_sec;
      times[1].tv_nsec = tm.tv_nsec;
    }

    if(i>0){
      delta = abs(
        ( ( (times[0].tv_sec)*1000 ) + ( (times[0].tv_nsec)/1000000) )
        -( ( (times[1].tv_sec)*1000 ) + ( (times[1].tv_nsec)/1000000 ) )
      );
      if(secret == 0 || delta < secret)
        secret = delta;
    }

    if( i == 0 ){
      long int idToconv1 = ntohl((netid>>32));
      long int idToconv2 = ntohl((int)netid);
      hostid = ( ( ( ( uint64_t ) idToconv1) <<32 ) | idToconv2 );
    }
    printf("SERVER %d INCOMING FROM %lx @%ld seconds %ld nsec\n",
      k, hostid, tm.tv_sec, tm.tv_nsec);
    fflush(stdout);
    i++;
  }
  //potrebbe succedere che un client si connette al server
  //ma non lo seleziona successivamente nell'invio del messaggio
  //in questo caso il server non invia nulla al supervisor
  if(i){
    printf("SERVER %d CLOSING %lx ESTIMATE %d\n", k, hostid, secret );
    fflush(stdout);
    if(running){
      pthread_mutex_lock(&mutex);
      if( !running || write( pipeaddr, &hostid, sizeof(hostid) ) == -1){
        pthread_mutex_unlock(&mutex);
        perror("Write failed or supervisor closed ");
        exit(EXIT_FAILURE);
      }
      if( !running || write( pipeaddr, &secret, sizeof(secret) ) == -1){
        pthread_mutex_unlock(&mutex);
        perror("Write failed ");
        exit(EXIT_FAILURE);
      }
      pthread_mutex_unlock(&mutex);
    }
    else{
      printf("SERVER %d NOT RECEIVED ANY MESSAGE\n", k);
    }
  }
  //socket closed
  close(fdc);
  pthread_exit(NULL);
}

static void gestore(int num){
  running = 0;
}

static void thread_cleanup_handler(){
  pthread_mutex_unlock(&mutex);
}
