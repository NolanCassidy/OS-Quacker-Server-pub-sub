#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#include <sys/wait.h>
#include "circular_buffer.c"
// #include "quacker.h"

pid_t quacker_pid;

/* Helper functions for printing all of the relevent
   pub and sub processes in your program. You may not
   need to use these.                                */
void print_pubs(struct pub_process pubs[], int num_pubs);
void print_subs(struct sub_process subs[], int num_subs);


/* subscriber and publisher interact with the Quacker
   backend, QuackIt. Currently, they send their ID, and then
   terminate. Use these functions as a template for modeling
   more complex interactions between Quacker users and QuackIt */
void subscriber(struct sub_process sub_proc);
void publisher(struct pub_process pub_proc);

/* These print functions are usefull for identifing which output
   came from which of the running processes. Pass the PID and the
   string you want to print from a publisher or a subscriber to the
   appropriate function                                             */
void pub_print(int pid, char * str){ printf("\n[Pub process %d] %s\n", pid, str);}
void sub_print(int pid, char * str){ printf("\n[Subscriber process %d] %s\n", pid, str);}

int main(int argc, char * argv[]){

    printf("total args %d\n", argc);
    int i;
    for (i = 0; i < argc; i++)
    {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    if(argc != 3)
    {
        fprintf(stderr, "Error, bad args.\n USAGE: startQuacker [num_publisheds] [num_subscribers]\n");
    }

    int TOTAL_PUBS = atoi(argv[1]);
    printf("total pubs : %d\n", TOTAL_PUBS);

    int TOTAL_SUBS = atoi(argv[2]);
    printf("total subs : %d\n", TOTAL_SUBS);

    struct pub_process pubs[TOTAL_PUBS];
    struct sub_process subs[TOTAL_SUBS];

    // fork off all of the pub processes
    // keep a file desscriptor for each publisher
    pid_t pid;
    for (i  = 0; i < TOTAL_PUBS; i++)
    {
        pipe(pubs[i].pub_to_quacker);
        pipe(pubs[i].quacker_to_pub);
        pubs[i].pid = 0;
        pid = fork();

        // Safety check
        if (pid < 0)
        {
            fprintf(stderr, "Fork Failed.\n");
            exit(0);
        }

        // Parent updates pubs data structure
        else if (pid > 0)
        {
            printf("Forked publisher with ID : %d\n", pid);
            pubs[i].pid = pid;
        }

        // child executes publisher function, then creation loop
        else if (pid == 0 )
        {
            publisher(pubs[i]);
            break;
        }
    }

    // Now parent will spawn off subscribers
    if (pid > 0)
    {
        for (i  = 0; i < TOTAL_SUBS; i++)
        {
            pipe(subs[i].sub_to_quacker);
            pipe(subs[i].quacker_to_sub);
            subs[i].pid = 0;
            pid = fork();

            // Safety check
            if (pid < 0)
            {
                fprintf(stderr, "Fork Failed.\n");
                exit(0);
            }
            // Parent updates subs data structure
            else if (pid > 0)
            {
                printf("Forked Subscriber with ID : %d\n", pid);
                subs[i].pid = pid;
            }
            // child escepes the creation loop
            else if (pid == 0 )
            {
                subscriber(subs[i]);
                break;
            }
        }
    }

    if (pid > 0)
    {
        // Fork to create sever request handler.
        quacker_pid = fork();
        if (quacker_pid == 0)
        {
            // Quacker procs created. Call circularBuffer to start the QuackIt Server.
            circular_buffer(TOTAL_PUBS, pubs, TOTAL_SUBS, subs);
        }

        else if (quacker_pid > 0)
        {
            // Parent process waits for all child procs to complete
            for(i=0; i < TOTAL_SUBS; ++i)
            {
              pid = fork();
              if (pid == 0){
                break;
              }
            }
        }

        else if (quacker_pid < 0)
        {
            fprintf(stderr, "Error creating Quacker Server.");
            exit(0);
        }
    }

    // Wait for all child procs to complete to complete
    if (pid > 0){
        int status = 0;
        pid_t child_pid;
        while( (child_pid = wait(&status)) > 0 );
    }


    return 0;
}

void print_pubs(struct pub_process pubs[], int num_pubs)
{
    int i;
    for(i = 0; i < num_pubs; i++)
    {
        fprintf(stdout, "pubs[%d].pid = %d\n", i, (int)pubs[i].pid);
    }
    printf("\n");
}
void print_subs(struct sub_process subs[], int num_subs)
{
    int i;
    for(i = 0; i < num_subs; i++)
    {
        fprintf(stdout, "subs[%d].pid = %d\n", i, (int)subs[i].pid);
    }
    printf("\n");
}

void publisher(struct pub_process pub_proc)
{
    char message[QUACK_SIZE];
    char response[QUACK_SIZE];
    char print_buffer[QUACK_SIZE];

    //while(pub_proc.pid == 0); // wait for pid to be set
    int pub_id = getpid();
    sprintf(message, "pub %d connect", pub_id);

    // close reading end of pub_to_quacker
    close(pub_proc.pub_to_quacker[0]);

    // close writing end of quacker_to_pub
    close(pub_proc.quacker_to_pub[1]);

    pub_print(pub_id, "sending message");
    write(pub_proc.pub_to_quacker[1], message, strlen(message)+1);

    pub_print(pub_id, "waiting for response");
    read(pub_proc.quacker_to_pub[0], response, QUACK_SIZE);

    sprintf(print_buffer, "Got message: %s", response );
    pub_print(pub_id, print_buffer);

    sprintf(message, "end");


    while(strstr(response, "terminate") == NULL)
    {
        sprintf(print_buffer, "Sending Message: %s", message);
        pub_print(pub_id, print_buffer);

        write(pub_proc.pub_to_quacker[1], message, strlen(message)+1);

        pub_print(pub_id, "waiting for response");

        read(pub_proc.quacker_to_pub[0], response, QUACK_SIZE);

        sprintf(print_buffer, "Got message: %s", response);
        pub_print(pub_id, print_buffer);

        sprintf(message, "terminate");
    }
}

void subscriber(struct sub_process sub_proc)
{
    sleep(2); // allow pub process to 'go' first. You can delete this line.
    char message[QUACK_SIZE];
    char response[QUACK_SIZE];
    char print_buffer[QUACK_SIZE];

    int sub_id = getpid();
    sprintf(message, "sub %d connect", sub_id);

    // close reading end of sub_to_quacker
    close(sub_proc.sub_to_quacker[0]);

    // close writing end of quacker_to_sub
    close(sub_proc.quacker_to_sub[1]);

    sub_print(sub_id, "sending message");
    write(sub_proc.sub_to_quacker[1], message, strlen(message)+1);

    sub_print(sub_id, "waiting for response");
    read(sub_proc.quacker_to_sub[0], response, QUACK_SIZE);

    sprintf(print_buffer, "Got message: %s", response );
    sub_print(sub_id, print_buffer);

    sprintf(message, "end");


    while(strstr(response, "terminate") == NULL)
    {
        sprintf(print_buffer, "Sending Message: %s", message);
        sub_print(sub_id, print_buffer);

        write(sub_proc.sub_to_quacker[1], message, strlen(message)+1);

        sub_print(sub_id, "waiting for response");

        read(sub_proc.quacker_to_sub[0], response, QUACK_SIZE);

        sprintf(print_buffer, "Got message: %s", response);
        sub_print(sub_id, print_buffer);

        sprintf(message, "terminate");
    }
}
