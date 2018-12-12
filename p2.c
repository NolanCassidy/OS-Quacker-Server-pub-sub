#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>

int NUMTOPICS = 1;                // if no arguments are enterend then default to 1,1,1,1000
int TOTAL_PUBS = 1;
int TOTAL_SUBS = 1;
#define MAXENTRIES 1000           //max size of topic entries
#define QUACKSIZE 140             //max size of message

typedef struct topicentry
{
    int entrynum;
    int timestamp;
    int pubID;
    int message[QUACKSIZE];
}
topicentry;

typedef struct queue
{
        int maxentry;
        struct topicentry *circular_buffer;
        int in;
        int out;
        int topic_counter;

}
queue;

struct process                  //instead of seperare pub/sub structs there is one where puborsub == "sub" or "pub"
{
 int pid;
 char *puborsub;
 int *in_to_out;
 int *out_to_in;
};

queue *IOBUFFER1;
queue *IOBUFFER2;
queue *Topic_Q_ptr[MAXENTRIES];

struct topicentry read_post(queue *Q_ptr)
{
        if(Q_ptr->topic_counter==0)
        {
                //no topics to read
                exit(0);
        }
        return Q_ptr->circular_buffer[Q_ptr->in];
}

queue *make_queue(int maxentries)
{
        queue *Q_ptr;
        Q_ptr = (queue *)malloc(sizeof(queue));

        Q_ptr->maxentry = maxentries;
        Q_ptr->circular_buffer = (topicentry *)malloc(sizeof(int)*maxentries);
        Q_ptr->in = 0;
        Q_ptr->out = -1;
        Q_ptr->topic_counter = 0;

        return Q_ptr;
}

void enqueue(queue *Q_ptr, struct topicentry entry)
{
        if(Q_ptr->topic_counter == Q_ptr->maxentry)
        {
            fprintf(stdout,"retry");
            return;
        }
        Q_ptr->out++;
        Q_ptr->topic_counter++;
        if(Q_ptr->maxentry == Q_ptr->out)
        {
          Q_ptr->out = 0;
        }
        Q_ptr->circular_buffer[Q_ptr->out].pubID = entry.pubID;
        Q_ptr->circular_buffer[Q_ptr->out].timestamp = entry.timestamp;

        for(int i=0;i<QUACKSIZE;i++)
        {
            Q_ptr->circular_buffer[Q_ptr->out].message[i] = entry.message[i];
        }

        return;
}

void dequeue(queue *Q_ptr)
{
   if(Q_ptr->topic_counter==0)
   {
      fprintf(stdout,"reject");
      return;
    }

     if (read_post(Q_ptr).timestamp > 120)
     {
      if (IOBUFFER1->topic_counter > 60)
      {
       enqueue(IOBUFFER2,read_post(Q_ptr));
      }
      else
      {
       enqueue(IOBUFFER1,read_post(Q_ptr));
      }
     }

     Q_ptr->in++;
     Q_ptr->topic_counter--;
     if(Q_ptr->in==Q_ptr->maxentry)
     {
        Q_ptr->in=0;
     }

     return;
}

void make_buffer(int connections[(TOTAL_PUBS + TOTAL_SUBS)][2])
{
 for (int i=0 ; i<(TOTAL_PUBS + TOTAL_SUBS) ; i++)
 {
  pipe(connections[i]);
 }
}

void *read_buffer(int connection[2])
{
   void* buffer=NULL;
 int size_buffer=0;
 if (ioctl(connection[0], FIONREAD, &size_buffer) != -1)
 {
    buffer = malloc(size_buffer);
    read(connection[0], buffer, size_buffer);
    return buffer;
  }
    //failed to read
    exit(0);
}

void publisher(int pid, int connection[2], int server_busy[])
{
   char* buffer=NULL;
  char message[64];

 server_busy[pid] = 0;

 for(int i = 0; i<=4; i++){
   if(server_busy[pid] == 1)
   {
     while(server_busy[pid]);
     buffer = read_buffer(connection);
     fprintf(stdout,"pub %d %s\n",pid,buffer);
     if(i!=4)
     {
      server_busy[pid] = 0;
     }
   }
    if(server_busy[pid] == 0)
    {
      if(i==0){
        sprintf(message,"pub connect");
      }else if(i==1)
      {
        sprintf(message,"topic %d %d",0,QUACKSIZE);
      }else if(i==2)
      {
        sprintf(message,"end");
      }else if(i==3)
      {
        sprintf(message,"terminate");
      }
      write(connection[1], message, sizeof(message));
    }
   server_busy[pid] = 1;
 }

 close(connection[1]);
 close(connection[0]);
 exit(0);
}

void subscriber(int pid, int connection[2], int server_busy[])
{
   char* buffer = NULL;
 char message[64];

 server_busy[pid] = 0;
 for(int i = 0; i<=4; i++){
   if(server_busy[pid] == 1)
   {
     while(server_busy[pid]);
     buffer = read_buffer(connection);
     fprintf(stdout,"sub %d %s\n",pid,buffer);
     if(i!=2 && i!=3)
     {
      server_busy[pid] = 0;
     }
   }
    if(server_busy[pid] == 0)
    {
      if(i==0){
        sprintf(message,"sub connect");
      }else if(i==1)
      {
         sprintf(message,"sub topic %d",0);
      }else if(i==4)
      {
         sprintf(message,"successful");
      }
      write(connection[1], message, sizeof(message));
    }
 server_busy[pid] = 1;
}
 while(server_busy[pid]);
server_busy[pid] = 0;
for(int i = 0; i<=2; i++){
  if(server_busy[pid] == 1)
  {
    while(server_busy[pid]);
    buffer = read_buffer(connection);
    fprintf(stdout,"sub %d %s\n",pid,buffer);
    if(i!=2)
    {
     server_busy[pid] = 0;
    }
  }
   if(server_busy[pid] == 0)
   {
     if(i==0){
        sprintf(message,"end");
     }else if(i==1)
     {
        sprintf(message,"terminate");
     }
     write(connection[1], message, sizeof(message));
   }
  server_busy[pid] = 1;
}

 close(connection[1]);
 close(connection[0]);

 exit(0);
}


void help_thread(struct process *p, char *buffer)
{
  while(!p->out_to_in[p->pid]);
  buffer = read_buffer(p->in_to_out);
  fprintf(stdout,"topic %d %s\n",p->pid,buffer);
}

void *threads(void *args)
{
  struct process *p = (struct process *)args;
   int check = strcmp(p->puborsub,"pub");

  char* buffer = NULL;
  char message[64];
  int topicNum;
  topicentry tempEntry;
  for(int i = 0;i<4;i++)
  {
    help_thread(p,buffer);
    if(i==0)
    {
      write(p->in_to_out[1], "accept", sizeof("accept"));
      p->out_to_in[p->pid] = 0;

      while(!p->out_to_in[p->pid]);
      buffer = read_buffer(p->in_to_out);
      fprintf(stdout,"topic %d %s\n",p->pid,buffer);
    }else if (i==1)
    {
        if(check)
        {
          write(p->in_to_out[1], "accept", sizeof("accept"));
          p->out_to_in[p->pid] = 0;
          sscanf(buffer, "sub topic %d", &topicNum);
          int check_ptr = Topic_Q_ptr[0]->topic_counter;
          while (check_ptr == 0);
          while(!p->out_to_in[p->pid]);
          sprintf(message,"topic %d %d",0,QUACKSIZE);
        }else
        {
          sscanf(buffer, "topic %d", &topicNum);
          tempEntry.pubID = p->pid;
          tempEntry.timestamp = 120;
          for(int c=0;c<QUACKSIZE;c++)
          {
            tempEntry.message[0] = c*c;
          }
          enqueue(Topic_Q_ptr[topicNum],tempEntry);
          sprintf(message, "successful");
        }

        write(p->in_to_out[1], message, sizeof(message));
        p->out_to_in[p->pid] = 0;
    }else if (i==2)
    {
      if(check)
      {
        printtopicQ(Topic_Q_ptr,topicNum,p->pid);
        dequeue(Topic_Q_ptr[topicNum]);

        p->out_to_in[p->pid]=0;

    help_thread(p,buffer);
      }
      write(p->in_to_out[1], "accept", sizeof("accept"));
      p->out_to_in[p->pid] = 0;

    }else if (i==3)
    {
      write(p->in_to_out[1],"terminate",sizeof("terminate"));
      p->out_to_in[p->pid] = 0;

    }
  }
  close(p->in_to_out[1]);
  close(p->in_to_out[0]);

  pthread_exit(NULL);
  return 0;
}

void make_topic_q(){
  int i;
  for (i=0 ; i < NUMTOPICS ; i++)
  {
   Topic_Q_ptr[i] = make_queue(MAXENTRIES);
  }
}

void create_quacker_server(int connections[(TOTAL_PUBS + TOTAL_SUBS)][2], int out_to_in[])
{
  struct process p[(TOTAL_PUBS + TOTAL_SUBS)];
  pthread_t ServerThreadProxies[(TOTAL_PUBS + TOTAL_SUBS)];

 printf("Program has began running using:\n");
 printf("%d publishers\n", TOTAL_PUBS);
 printf("%d subscribers\n", TOTAL_SUBS);
 printf("%d topics\n", NUMTOPICS);
 printf("%d max topic entries\n\n", MAXENTRIES);

 IOBUFFER1 = make_queue(1);
 IOBUFFER2 = make_queue(1);
 make_topic_q();

 for (int i=0 ; i < (TOTAL_PUBS + TOTAL_SUBS) ; i++)
 {
   if (i < TOTAL_PUBS)
   {
    p[i].puborsub = "pub";
   }else
   {
    p[i].puborsub = "sub";
   }

    p[i].in_to_out = connections[i];
    p[i].out_to_in = out_to_in;
    p[i].pid = i;

  pthread_create(&(ServerThreadProxies[i]), NULL, &threads, (void *)&p[i]);
 }

 for (int i=0 ; i < (TOTAL_PUBS + TOTAL_SUBS) ; i++)
 {
  pthread_join(ServerThreadProxies[i], NULL);
 }

 fprintf(stdout,"\nProgram successfully finished running.\n");
 exit(0);
}

void printtopicQ(queue *Topic_Q_ptr[NUMTOPICS], int topicNum, int sub_id)
{
 if (Topic_Q_ptr[topicNum]->topic_counter <= 0)
 {
   //empty queue
   return;
 }

  printf("\n===================================================\n");
  printf("sub %d \n",sub_id);
  printf("pub %d \n",read_post(Topic_Q_ptr[topicNum]).pubID);
  printf("timestamp %d \n",read_post(Topic_Q_ptr[topicNum]).timestamp);
  printf("topic %d %d\n",read_post(Topic_Q_ptr[topicNum]).message[0],read_post(Topic_Q_ptr[topicNum]).message[1]);
  printf("===================================================\n");

}

void *archive()
{
  FILE *f = fopen("archive.txt", "w");

 while(IOBUFFER1->topic_counter > 0)
 {
  fprintf(f, "\n pubid %d timeStamp %d topic %d %d\n",read_post(IOBUFFER1).pubID,read_post(IOBUFFER1).timestamp,read_post(IOBUFFER1).message[0],read_post(IOBUFFER1).message[1]);

  IOBUFFER1->in++;
  IOBUFFER1->topic_counter--;


   if(IOBUFFER1->in==IOBUFFER1->maxentry)
   {
     IOBUFFER1->in=0;
   }
 }
 exit(0);
}

 int shmid;
   int *shm;


void make_shared_mem()
{
  //http://man7.org/linux/man-pages/man2/shmget.2.html
   shmid = shmget(IPC_PRIVATE, (TOTAL_PUBS + TOTAL_SUBS)*sizeof(int), IPC_CREAT | 0666);
    shm = (int *) shmat(shmid, NULL, 0);
    int j;
    for (j=0 ; j<(TOTAL_PUBS + TOTAL_SUBS) ; ++j){
     shm[j] = 0;
    }
}

void clear_shared_mem()
{
  shmdt((void *) shm);
  shmctl(shmid, IPC_RMID, NULL);
}

int main(int argc, char * argv[])
{
  //you can enter args for pub,sub, and topic amount or they default to 1
  if(argc == 4)
  {
    TOTAL_PUBS = atoi(argv[1]);
    TOTAL_SUBS = atoi(argv[2]);
    NUMTOPICS = atoi(argv[3]);
  }else if(argc==3){
    TOTAL_PUBS = atoi(argv[1]);
    TOTAL_SUBS = atoi(argv[2]);
  }

make_shared_mem();
pthread_t thread;
 int connections[(TOTAL_PUBS + TOTAL_SUBS)][2];
 make_buffer(connections);

 pid_t quacker_pid = fork();

 if (quacker_pid == 0)
 {
    create_quacker_server(connections,shm);
    if (IOBUFFER1->topic_counter == IOBUFFER1->maxentry)
    {
     pthread_create(&thread, NULL, &archive, NULL);
     pthread_join(thread, NULL);
    }
 }
 if (quacker_pid > 0)
 {
    int i;
    for(i=0; i < TOTAL_PUBS; i++)
    {
       quacker_pid = fork();
       if (quacker_pid == 0)
       {
        publisher(i,connections[i],shm);
        break;
       }
    }
    if (quacker_pid > 0)
    {
        for(i=0; i < TOTAL_SUBS; i++)
        {
            quacker_pid = fork();
            if (quacker_pid == 0)
            {
              subscriber(i+TOTAL_PUBS,connections[i+TOTAL_PUBS],shm);
              break;
            }
        }
    }
  }

clear_shared_mem();
 return 0;
}
