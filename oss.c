/*  Author: Chase Richards
    Project: Homework 6 CS4760
    Date April 23, 2020
    Filename: oss.c  */

#include "oss.h"

char *outputLog = "logOutput.dat";

int main(int argc, char* argv[])
{
    int c;
    int n = 18; //Max Children in system at once
    int m = 1;
    srand(time(0));
    while((c = getopt(argc, argv, "hn:m:")) != -1)
    {
        switch(c)
        {
            case 'h':
                displayHelpMessage();
                return (EXIT_SUCCESS);
            case 'n':
                n = atoi(optarg);
                break;
            case 'm':
                m = atoi(optarg);
                break;
            default:
                printf("Using default values");
                break;               
        }
    }
  
    /* Signal for terminating, freeing up shared mem, killing all children 
       if the program goes for more than two seconds of real clock */
    signal(SIGALRM, sigHandler);
    alarm(2);

    /* Signal for terminating, freeing up shared mem, killing all 
       children if the user enters ctrl-c */
    signal(SIGINT, sigHandler);  
    
    manager(n, m); //Call scheduler function
    removeAllMem(); //Remove all shared memory, message queue, kill children, close file

    return 0;
}

/* Does the fork, exec and handles the messaging to and from user */
void manager(int maxProcsInSys, int memoryScheme)
{
    filePtr = openLogFile(outputLog); //open the output file
    
    int receivedMsg; //Recieved from child telling scheduler what to do
    
    /* Create the simulated clock in shared memory */
    clksim *clockPtr;
    clockSegment = shmget(clockKey, sizeof(clksim), IPC_CREAT | 0666);
    if(clockSegment < 0)
    {
        perror("oss: Error: Failed to get clock segment (shmget)");
        removeAllMem();
    }
    clockPtr = shmat(clockSegment, NULL, 0);
    if(clockPtr < 0)
    {
        perror("oss: Error: Failed to attach clock segment (shmat)");
        removeAllMem();
    }
    clockPtr-> sec = 0;
    clockPtr-> nanosec = 0;   
 
    /* Constant for the time between each new process and the time for
       spawning the next process, initially spawning one process */   
    clksim maxTimeBetweenNewProcesses = {.sec = 0, .nanosec = 500000};
    clksim spawnNextProc = {.sec = 0, .nanosec = 0};

    /* Create the message queue */
    msgqSegment = msgget(messageKey, IPC_CREAT | 0777);
    if(msgqSegment < 0)
    {
        perror("oss: Error: Failed to get message segment (msgget)");
        removeAllMem();
    }
    msg message;

    sem_t *sem;
    char *semaphoreName = "semOss";

    int outputLines = 0; //Counts the lines written to file to make sure we don't have an infinite loop
    int procCounter = 0; //Counts the processes

    //Statistics
    int totalProcs = 0;
    int memAccesses = 0;
    int pageFaults = 0;

    int i, j; //For loops
    int processExec; //exec  nd check for failurei
    int procPid; //generated pid
    int pid; //actual pid
    char msgqSegmentStr[10]; //for execing to the child
    char procPidStr[3]; //for execing the generated pid to child
    char schemeStr[2]; //for execing the scheme to child
    sprintf(msgqSegmentStr, "%d", msgqSegment);
    sprintf(schemeStr, "%d", memoryScheme);
    //Array of the pids in the generated pids index, initialized to -1 for available
    int *pidArr;
    pidArr = (int *)malloc(sizeof(int) * maxProcsInSys);
    for(i = 0; i < maxProcsInSys; i++)
        pidArr[i] = -1;

    //Allocate the frame table as an array
    frameTable *frameT = (frameTable *)malloc(sizeof(frameTable) * 256);
    //Initialize the frame table
    for(i = 0; i < 256; i++)
    {
        //Not occupied yet
        frameT[i].referenceBit = 0x0;
        frameT[i].dirtyBit = 0x0;
        frameT[i].process = -1;
    }
    //Allocate the page tables as an array
    pageTable *pageT = (pageTable *)malloc(sizeof(pageTable) * maxProcsInSys);
    //Initialize the page tables
    for(i = 0; i < 18; i++)
    {
        for(j = 0; j < 32; j++)
            pageT[i].pageArr[j].locationOfFrame = -1;
    }
   

    //Printing the inital allocated matrix showing that nothing is allocated
    fprintf(filePtr, "Program Starting\n");
    outputLines++;

    printf("ProcCounter: %d", procCounter);
    //Loop runs constantly until it has to terminate
    while(procCounter <= 2)
    {
        //Only 18 processes in the system at once and spawn random between 0 and 500000
        if((procCounter < maxProcsInSys) && ((clockPtr-> sec > spawnNextProc.sec) || (clockPtr-> sec == spawnNextProc.sec && clockPtr-> nanosec >= spawnNextProc.nanosec)))
        {
            procPid = generateProcPid(pidArr, maxProcsInSys);
            if(procPid < 0)
            {
                perror("oss: Error: Max number of pids in the system\n");
                removeAllMem();               
            }     
            /* Copy the generated pid into a string for exec, fork, check
               for failure, execl and check for failure */      
            sprintf(procPidStr, "%d", procPid);
            pid = fork();
            if(fork < 0)
            {
                perror("oss: Error: Failed to fork process\n");
                removeAllMem();
            }
            else if(pid == 0)
            {
                processExec = execl("./user", "user", msgqSegmentStr, procPidStr, schemeStr, (char *)NULL);
                if(processExec < 0)
                {
                    perror("oss: Error: Failed to execl\n");
                    removeAllMem();
                }
            }
            totalProcs++;
            procCounter++;
            //Put the pid in the pid array in the spot based on the generated pid
            pidArr[procPid] = pid;
     
            //Get the time for the next process to run
            spawnNextProc = nextProcessStartTime(maxTimeBetweenNewProcesses, (*clockPtr));
   
            clockIncrementor(clockPtr, 1000000); 
                
            
        }
        else if((msgrcv(msgqSegment, &message, sizeof(msg), 1, IPC_NOWAIT)) > 0) 
        {
            //Increment the clock for the read/write operation
            clockIncrementor(clockPtr, 15);
            memAccesses++;
            //Check to see if the frame is available in the page table
            
        } 
    }    
    
    
    return;  
}

//Generate a simulated process pid between 0-17
int generateProcPid(int *pidArr, int totalPids)
{
    int i;
    for(i = 0; i < totalPids; i++)
    {
        if(pidArr[i] == -1)
            return i;
    }
    return -1;
}

/* Determines when to launch the next process based on a random value
   between 0 and maxTimeBetweenNewProcs and returns the time that it should launch */
clksim nextProcessStartTime(clksim maxTimeBetweenProcs, clksim curTime)
{
    clksim nextProcTime = {.sec = (rand() % (maxTimeBetweenProcs.sec + 1)) + curTime.sec, .nanosec = (rand() % (maxTimeBetweenProcs.nanosec + 1)) + curTime.nanosec};
    if(nextProcTime.nanosec >= 1000000000)
    {
        nextProcTime.sec += 1;
        nextProcTime.nanosec -= 1000000000;
    }
    return nextProcTime;
}

/* For sending the message to the processes */
void messageToProcess(int receivingProcess, int response)
{
    int sendmessage;
    //Process is -1 because we didn't generate one, no resource, oss is sending process of 1
    msg message = {.typeofMsg = receivingProcess, .msgDetails = response, .process = -1, .address = -1};
    
    //Send the message and check for failure
    sendmessage = msgsnd(msgqSegment, &message, sizeof(msg), 0);
    if(sendmessage == -1)
    {
        perror("oss: Error: Failed to send message (msgsnd)\n");
        removeAllMem();
    }
    return;
}

/* Print Allocation Matrix according to the assignment sheet */
void printAllocatedTable(int allocated2D[18][20], int processes, int resources)
{
    int mRow, mColumn;
    fprintf(filePtr, "Allocated Matrix\n   ");
    //Print the resource column names R0-R19
    for(mColumn = 0; mColumn < resources; mColumn++)
    {
        fprintf(filePtr, "R%-2d ", mColumn);
    }
    fprintf(filePtr, "\n");
    //Print the process row names P0-P17
    for(mRow = 0; mRow < processes; mRow++)
    {
        fprintf(filePtr, "P%-2d ", mRow);
        //Loop through each spot in the table
        for(mColumn = 0; mColumn < resources; mColumn++)
        {
            //If the spot is not allocated, print 0
            if(allocated2D[mRow][mColumn] == 0)
                fprintf(filePtr, "0   ");
            //Otherwise print the allocated value
            else
                fprintf(filePtr, "%-3d ", allocated2D[mRow][mColumn]);           
        }
        fprintf(filePtr, "\n");
    }
    return;
}

/* Function for printing the allocated values (will be called roughly every 20 granted requests)
void printTable(resDesc resDescPtr, int processes, int resources)
{
    printAllocatedTable(resDescPtr.allocated2D, processes, resources);
    return;
}
*/

/* Open the log file that contains the output and check for failure */
FILE *openLogFile(char *file)
{
    filePtr = fopen(file, "a");
    if(filePtr == NULL)
    {
        perror("oss: Error: Failed to open output log file");
        exit(EXIT_FAILURE);
    }
    return filePtr;
}

/* When there is a failure, call this to make sure all memory is removed */
void removeAllMem()
{
    //shmctl(resDescSegment, IPC_RMID, NULL);   
    shmctl(clockSegment, IPC_RMID, NULL);
    msgctl(msgqSegment, IPC_RMID, NULL);
    sem_unlink("semOss");
    sem_unlink("semUser");
    kill(0, SIGTERM);
    fclose(filePtr);   
    exit(EXIT_SUCCESS);
} 

/* Print the final statistics to the console and the end of the file - will be called in signal handler
void printStats()
{
    printf("Total Granted Requests: %d\n", granted);
    printf("Total Normal Terminations: %d\n", normalTerminations);
    printf("Total Deadlock Algorithm Runs: %d\n", deadlockAlgRuns);
    printf("Total Deadlock Terminations: %d\n", deadlockTerminations);
    printf("Pecentage of processes in deadlock that had to terminate on avg: %d%\n", divideNums(deadlockTerminations, totalCounter));
}
*/

/* Signal handler, that looks to see if the signal is for 2 seconds being up or ctrl-c being entered.
   In both cases, print the final statistics and remove all of the memory, semaphores, and message queues */
void sigHandler(int sig)
{
    //printStats();
    if(sig == SIGALRM)
    {
        //printStats();
        printf("Timer is up.\n"); 
        printf("Killing children, removing shared memory, semaphore and message queue.\n");
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
    
    if(sig == SIGINT)
    {
        printf("Ctrl-c was entered\n");
        printf("Killing children, removing shared memory, semaphore and message queue\n");
        removeAllMem();
        exit(EXIT_SUCCESS);
    }
}


/* For the -h option that can be entered */
void displayHelpMessage() 
{
    printf("\n---------------------------------------------------------\n");
    printf("See below for the options:\n\n");
    printf("-h    : Instructions for running the project and terminate.\n"); 
    printf("-n x  : Number of processes allowed in the system at once (Default: 18).\n");
    printf("-m x  : Determines how a child will perform their memory access (Default: 0).\n");
    printf("\n---------------------------------------------------------\n");
    printf("Examples of how to run program(default and with options):\n\n");
    printf("$ make\n");
    printf("$ ./oss\n");
    printf("$ ./oss -n 10 -m 1\n");
    printf("$ make clean\n");
    printf("\n---------------------------------------------------------\n"); 
    exit(0);
}
