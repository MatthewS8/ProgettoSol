// MATTEO STEFANELLI MAT: 543781
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <signal.h>


static void gestore (){
  printf("Servers closed, closing client\n");
  exit(0);
}

void** toFree;
int dimension = 0;

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
  if(argc != 4){
    fprintf(stderr, "Numero parametri diverso da 3\n");
    exit(EXIT_FAILURE);
  }
  int k = atoi(argv[2]);
  int p = atoi(argv[1]);
  if(p>k || p<1){
    fprintf(stderr, "p deve essere compreso tra 1 e k\n");
    exit(EXIT_FAILURE);
  }
  int w = atoi(argv[3]);
  if(w < 3*p){
    fprintf(stderr, "w deve essere maggiore di 3p\n");
    exit(EXIT_FAILURE);
  }

  atexit(cleanup);

  struct sigaction sigpipe;
  memset(&sigpipe, 0, sizeof(sigpipe));
  sigpipe.sa_handler = gestore;
  sigaction(SIGPIPE, &sigpipe, NULL);

  srand(time(NULL) * getpid());
  int secret = (rand()%3000)+1;
  struct timespec tspec;
  clock_gettime(CLOCK_REALTIME,&tspec);
  srand48(tspec.tv_nsec);
  uint64_t id =( (uint64_t) lrand48() << 32 ) | lrand48();
  printf("CLIENT %lx SECRET %d\n", id, secret);
  fflush(stdout);

  int *fd_skt = malloc(sizeof(int)*p);
  cleanup_push(fd_skt);
  //generazione dei server a cui connettersi
  int *servers = malloc(sizeof(int)*p); //servers a cui connettersi [1, k]
  cleanup_push(servers);
  int i;
  struct sockaddr_un *sa = malloc(sizeof(struct sockaddr_un)*p);//path del server i
  cleanup_push(sa);
  i = 0;
  while(i < p){
    int nr = ((rand()%k) + 1), j;
    //controllo  che il server non sia stato gia' selezionato
    for(j=0; j<i && servers[j]!=nr; j++){}
    
    if(j>=i){
      servers[i] = nr;
      snprintf((sa[i]).sun_path, 16, "./OOB-server-%d", servers[i]);
      (sa[i]).sun_family = AF_UNIX;
      fd_skt[i] = socket(AF_UNIX,SOCK_STREAM,0);
      if(fd_skt[i] == -1){
        perror("SOCKET ERROR: ");
        exit(EXIT_FAILURE);
      }
      fflush(stdout);
      int fdc = connect(fd_skt[i],(struct sockaddr*) &(sa[i]), sizeof(sa[i]));
      i++;
      if(fdc == -1){
        perror("SOCKET CONNECT ERROR:  ");
        exit(EXIT_FAILURE);
      }
    }
  }
  //Converto id in Network Byte Order
  long int id_conv1 = htonl((long int)(id >> 32));
  long int id_conv2 = htonl((long int) id);
  uint64_t id_conv = ((((uint64_t) id_conv1) << 32) |id_conv2);

  for(i=0; i<w; i++){
    //Ciclo di w messaggi
    int stc = rand()%p; //Socket To Connect
    if(write(fd_skt[stc], &id_conv, sizeof(id_conv)) == -1){
      perror("WRITE ERROR ");
      exit(EXIT_FAILURE);
    }

    struct timespec time_toWait;
    time_toWait.tv_nsec = (secret%1000) * 1000000;
    time_toWait.tv_sec = secret / 1000;
    nanosleep(&time_toWait,NULL);
  }

  for(i=0; i<p; i++){
    close(fd_skt[i]);
  }

  printf("CLIENT %lx DONE\n", id);
  fflush(stdout);
  return 0;
}
