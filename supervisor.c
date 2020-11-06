// MATTEO STEFANELLI MAT: 543781
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <poll.h>
#include <inttypes.h>
#include <signal.h>

#define LUNGHEZZA_MSG 58

typedef struct el{
  uint64_t id; int secret; int n_received;
}el;

struct timespec tint;
int dim_cl_secr = 0, k, *pids;
el* client_secrets;

void** toFree;
int dimension = 0;

void dati_clients(FILE *fd){
  int i;
  for(i=0; i<dim_cl_secr; i++){
    fprintf(fd,"SUPERVISOR ESTIMATE %d FOR %lx BASED ON %d\n",
      client_secrets[i].secret, client_secrets[i].id, client_secrets[i].n_received);
  }
  fflush(fd);
}

static void gestore(int num){
  if(tint.tv_sec == 0 && tint.tv_nsec == 0){
    clock_gettime(CLOCK_MONOTONIC, &tint);
    dati_clients(stderr);
  }else{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    int millisec = ((time.tv_sec * 1000) + (time.tv_nsec)/1000000) -
      ((tint.tv_sec * 1000) + (tint.tv_nsec)/1000000);
    if( millisec <= 1000){
      //TERMINAZIONE
      int count;
      for(count=0; count<k; count++)
        kill(pids[count],SIGUSR1);
      dati_clients(stdout);
      free(client_secrets);
      printf("SUPERVISOR EXITING\n");
      exit(0);
    }else{
      tint.tv_sec = time.tv_sec;
      tint.tv_nsec = time.tv_nsec;
      dati_clients(stderr);
    }
  }
}

void insertEl(el element){
    int i = 0;
    //vado ad aggiornare il campo solo se il nuvuo secret e' valido ( > 0)
    if(element.secret > 0){
      //verifico se c'e' match di id e nel caso prendo il secret minimo
      while( i < dim_cl_secr ){
        if( client_secrets[i].id == element.id ){
          if( client_secrets[i].secret > element.secret )
            client_secrets[i].secret = element.secret;
          (client_secrets[i].n_received)++;
          return;
        }
        i++;
      }
    //altrimenti aggiungo il nuovo id
    if(i == dim_cl_secr){
      dim_cl_secr++;
      client_secrets = (el*) realloc(client_secrets, sizeof(el) * dim_cl_secr );
      ( client_secrets[ dim_cl_secr - 1 ] ).id = element.id;
      ( client_secrets[ dim_cl_secr - 1 ] ).secret = element.secret;
      client_secrets[ dim_cl_secr - 1 ].n_received = 1;
      return;
    }
  }
  return;
}



static void cleanup(void){
  int i;
  for(i=0; i<dimension; i++){
    free(toFree[i]);
  }
  free(toFree);
}

static void cleanup_push(void* data){
  dimension++;
  toFree = realloc(toFree,sizeof(void *) * dimension);
  toFree[dimension -1] = data;
}

int main(int argc, char* argv[]){
  if(argc != 2){
    fprintf(stderr, "SUPERVISOR: Format is \"./Supervisor <k>\"\n");
    exit(EXIT_FAILURE);
  }

  atexit(cleanup);

  tint.tv_sec = 0;
  tint.tv_nsec = 0;

  struct sigaction s;
  memset(&s, 0, sizeof(s));
  s.sa_handler = gestore;
  s.sa_flags = SA_RESTART;
  sigaction(SIGINT, &s, NULL);

  k = atoi(argv[1]);
  int i;
  printf("SUPERVISOR STARTING %d\n", k);
  int **pipefd = malloc(sizeof( int* ) * k);
  for(i=0; i<k; i++){
    pipefd[i] = malloc(sizeof(int)*2);
    cleanup_push(pipefd[i]);
    if(pipe(pipefd[i]) == -1){
      fprintf(stderr, "IPER: pipe error\n");
      exit(EXIT_FAILURE);
    }
  }
  cleanup_push(pipefd);

  pids = malloc(sizeof(int)*k);
  cleanup_push(pids);

  for(i=0; i<k; i++){
    pids[i] = fork();
    if(pids[i] == 0){ //ramo figlio
      //chiusura canale lettura dei figli
      char nserver[6], npipefd[6];
      snprintf(nserver,6, "%d", i+1);
      snprintf(npipefd,6, "%d", pipefd[i][1]);
      int j;
      for(j=0; j<k; j++)
        if(j != i)
          close(pipefd[i][0]);
      if( execl("./server", "server", nserver, npipefd, (char*)NULL) == -1 ){
        perror("EXECL FAILED ");
        exit(EXIT_FAILURE);
      }
    }
  }
  //chiusura canale scrittura da parte dell'ipervisor
  for(i=0; i<k; i++)
    close(pipefd[i][1]);
  //SELECT
  fd_set readfds;

  while(1){
    FD_ZERO(&readfds);
    for(i=0; i<k; i++)
      FD_SET(pipefd[i][0],&readfds);
    int s = select( pipefd[k-1][0] + 1, &readfds, NULL, NULL, NULL);
    //lettura da pipe
    if(s){
      for( i=0; i<k && FD_ISSET(pipefd[i][0],&readfds) == 0; i++){}
      int r1 = 0, r2 = 0;
      el element;
      r1 = read(pipefd[i][0], &(element.id), sizeof(uint64_t));
      r2 = read(pipefd[i][0], &(element.secret), sizeof(int));
      if(r1 == sizeof(uint64_t) && r2 == sizeof(int) ){
        printf("SUPERVISOR ESTIMATE %d FOR %lx FROM %d\n", element.secret, element.id, i+1);
        fflush(stdout);
        insertEl(element);
      }else
        if(r1 == 0 || r2 == 0){
          //EOF
          close(pipefd[i][0]);
        }else{
          printf("ERROR ON READ");
          exit(EXIT_FAILURE);
        }
    }else{
      if(s < 0){
        perror("SELECT: ");
      }
    }
  }
  return 0;
}
