/*  Author: Chase Richards
    Project: Homework 6 CS4760
    Date April 23, 2020
    Filename: user.c  */

#include "oss.h"

int msgqSegment;
int main(int argc, char *argv[])
{
    msg message;
   
    clksim *clockPtr;    
 
    /* Get and attach to the clock simulation shared memory */
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0666);
    if(clockSegment < 0)
    {
        perror("user: Error: Failed to get clock segment (shmget)");
        exit(EXIT_FAILURE);
    }
    clockPtr = shmat(clockSegment, NULL, 0);
    if(clockPtr < 0)
    {
        perror("user: Error: Failed to attach clock (shmat)");
        exit(EXIT_FAILURE);
    }      
 
    //Will be used for messaging with oss
    int procPid, scheme;
    //Passed the message queue segment id through execl
    msgqSegment = atoi(argv[1]);
    //Passed the generated process id through execl
    procPid = atoi(argv[2]);
    //Passed the scheme type through execl
    scheme = atoi(argv[3]);
    //Memeory Reference counter
    int memReferences = 0; 
 
    //Random 
    srand(time(0));

    /*Continuous loop until it's time to terminate
    while(1)
    {
        
    }
    */

    messageToOss(procPid, firstScheme(), 0);

    return 0;
}

/* Message being sent to oss for read write or terminating */
void messageToOss(int curProcess, int address, int details)
{
    int sendmessage;
    /* Send the message to oss with type 1 and it's a request */
    msg message = {.typeofMsg = 20, .msgDetails = 2, .process = curProcess, .address = address};
    
    /* Send the message and check for failure */
    sendmessage = msgsnd(msgqSegment, &message, sizeof(msg), 0);
    if(sendmessage == -1)
    {
        perror("user: Error: Failed to send message (msgsnd)\n");
        exit(EXIT_FAILURE);
    }
        
    return;
}

int firstScheme()
{
    return rand() % 32768;
}
