/******************
* Davide Cerullo
* Edoardo Bazzotti
* 08/05/2023
******************/
#include "errExit.h"
#include "shared_memory.h"
#include "semaphore.h"

#include <stdio.h>
#include <stdbool.h>    // Bool
#include <stdlib.h>
#include <string.h>
#include <signal.h>     // Signal
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>    // Shared memory
#include <unistd.h>
#include <sys/wait.h>

#define TIMEOUT 30      // Alarm timeout in seconds

/* Define global variables */
key_t KEY = IPC_PRIVATE;       // Key for shared memory
key_t semKey = IPC_PRIVATE;    // Key for semaphore
int shmid;                     // Shared memory's ID
char *addr;                    // Shared memory as large as the matrix + char
int count_sig = 0;             // Counter of SIGINT
int semid;                     // Semaphore's ID
int fd;                        // Fifo file descriptor                 
int rows;                      // Argv[1]
int columns;                   // Argv[2]
pid_t pid;                     // Pid of son
bool isBot = false;            // Flag if there is a bot
pid_t pidBot = -1;             // Pid of Bot

// Struct to rappresent the players
typedef struct {
    pid_t pid1;
    pid_t pid2;
    char sign1;
    char sign2;
} t_players;
t_players players;

/* Define methods */
bool checkArgument(int argc, char const *argv[]);
bool closeAll(char *addr, int shmid);
void createGame();
void createSemaphore();
void sendFifo(const char *argv[]);
void sigHandler(int sig);
void sigHandlerUserOne(int sig);
void sigHandlerTimeOut(int sig);
void getClient();
void checkWin();
void sendStatus(int result);

int main(int argc, char const *argv[])
{
    /* Print error if the argument is incorrect*/
    if(argc != 5 || !checkArgument(argc, argv)) {
        printf("Usage: %s 5 5 O X\n", argv[0]);
        return 1;
    }

    // Change sigHandler for SIGINT --> Ctrl-C
    if (signal(SIGINT, sigHandler) == SIG_ERR) {
        printf("change signal handler failed\n");
    }

    // Change sigHandler for SIGUSR1 --> Client quitted
    if (signal(SIGUSR1, sigHandlerUserOne) == SIG_ERR) {
        printf("change signal handler failed\n");
    }

    // Change sigHandler for SIGALARM --> Timeout play
    if (signal(SIGALRM, sigHandlerTimeOut) == SIG_ERR){
        printf("change signal handler failed\n");
    }

    // Change sigHandler for SIGHUP --> Client terminated by closing terminal
    if (signal(SIGHUP, sigHandler) == SIG_ERR)
    {
        printf("change signal handler failed");
    }

    // ##### THE PARENT INITIALIZE SEM, SHM, FIFO AND GET CLIENT #####

    rows = atoi(argv[1]);
    columns = atoi(argv[2]);

    // Initialize shared memory
    createGame();

    // Initialize semaphore
    createSemaphore();
        
    // Both methods below have to stay boottom the semaphore initialization
    sendFifo(argv);  // Create and send FIFO message
    
    getClient();    // Get Clients PIDs

    // ##### THE SON MANAGE THE GAME #####

    if(pidBot != 0) {   // I'm the father
        pid = fork();
        
        if(pid == 0) {      // I'm the son

            /* BLOCK THE CHILD TO RECIVE SIGINT */
        
            sigset_t mySet, prevSet;
        
            // initialize mySet to contain all signals
            sigemptyset(&mySet);
            
            // add SIGINT to mySet
            sigaddset(&mySet, SIGINT);
            
            // blocking SIGINT
            sigprocmask(SIG_SETMASK, &mySet, &prevSet);

            while(1) {
                semOp(semid, 1, 1);     // V(giocatore1)
                printf("V(P1)\n");

                alarm(TIMEOUT);
                addr[rows*columns] = '5';
                semOp(semid, 0, -1);     // P(mutex)
                alarm(0);

                // Arbitrare
                checkWin();
                
                printf("ARBITRA\n");
                semOp(semid, 2, 1);     // V(giocatore2)
                printf("V(P2)\n");

                alarm(TIMEOUT);
                addr[rows*columns] = '4';
                semOp(semid, 0, -1);     // P(mutex)
                alarm(0);

                // Arbitrare
                checkWin();   
            }
        }
    }
    else {  // I'm the bot
    
        /* BLOCK THE BOT TO RECEIVE SIGINT */
        
        sigset_t mySet, prevSet;
    
        // initialize mySet to contain all signals
        sigemptyset(&mySet);
        
        // add SIGINT to mySet
        sigaddset(&mySet, SIGINT);
        
        // blocking SIGINT
        sigprocmask(SIG_SETMASK, &mySet, &prevSet);
        
        execl("./F4ClientBot", "./F4ClientBot", NULL);        
    }

    // wait the termination of all child processes
    while(wait(NULL) > 0);

    return 0;
}

/// @brief Function that check the parameters passed at the program
/// @param argc number of parameters
/// @param argv the parameters
/// @return true if correct otherwise false
bool checkArgument(int argc, char const *argv[]) {
    // Check matrix dimesion
    if(atoi(argv[1]) < 5 || atoi(argv[2]) < 5) {
        printf("Invalid matrix dimension! It must be at least 5\n");
        return false;
    }

    // Check character used
    if(strlen(argv[3]) != 1 || strlen(argv[4]) != 1) {
        printf("Invalid character length! It must be just one\n");
        return false;
    }

    // Check if symbols are equal
    if(!strcmp(argv[3], argv[4])) {
        printf("The Symbols choosen are equals!\n");
        return false;
    }

    return true;
}

/// @brief Function to create shared memory adn attach it
/// @param argv Length and height of the table
void createGame() {
    // Initialize game table
    char table[rows][columns];
    size_t size = sizeof(table) + sizeof(char);    // Share the table and the match result's
    
    // Initialize shared memory
    // Returns a shared memory segment identifier on success, or -1 on error
    shmid = alloc_shared_memory(KEY, size);

    // Attach segment to memory address space of the process
    addr = (char *)get_shared_memory(shmid, 0);

    // Copy matrix (empty) to shared memory
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < columns; j++){
            addr[j + (i*columns)] = '\0';
        }
    }
}

/// @brief Function to create and initialize 3 semaphores
void createSemaphore(){
    semid = semget(semKey, 3, IPC_CREAT | S_IRUSR | S_IWUSR);  // Create a semaphore set with 3 semaphores
    if (semid == -1)
        errExit("semget failed");

    // Initialize the semaphore set with semctl
    unsigned short semInitVal[] =  {0, 0, 0};       // mutex, giocatore1, giocatore2
    union semun arg;
    arg.array = semInitVal;

    if (semctl(semid, 0, SETALL, arg))
        errExit("semctl SETALL failed");
}

/// @brief Function to create and send information with FIFO
/// @param symbol1 Symbol to use for player 1
/// @param symbol2 Symbol to use for player 2
void sendFifo(const char *argv[]){
    char symbol1 = argv[3][0];
    char symbol2 = argv[4][0];
    players.sign1 = symbol1;
    players.sign2 = symbol2;

    /* Creating FIFO */
    // Check if FIFO already exists, remove it
    if(mkfifo("/tmp/share", S_IRUSR|S_IWUSR) == -1) {
        if(remove("/tmp/share") == -1 || mkfifo("/tmp/share", S_IRUSR|S_IWUSR) == -1)   // Creating FIFO file
            errExit("Error creating FIFO\n");
    }

    // Opening FIFO file
    fd = open("/tmp/share", O_WRONLY);
    if(fd == -1)
        errExit("Error opening FIFO\n");
    
    int player = 1;     // Variable to set player number

    // Writing on FIFO for first player
    write(fd, &shmid, sizeof(int));         // shmid
    write(fd, &semid, sizeof(int));         // semKey
    write(fd, &player, sizeof(int));        // Player number
    write(fd, &symbol1, sizeof(char));      // Symbol
    write(fd, &rows, sizeof(int));          // Length of table
    write(fd, &columns, sizeof(int));       // Height of table
    printf("Shmid: %d\n", shmid);
    printf("Semid: %d\n", semid);
    printf("Server: %d\n", KEY);

    // Writing on FIFO for second player
    player++;
    write(fd, &shmid, sizeof(int));         // shmid
    write(fd, &semid, sizeof(int));         // semKey
    write(fd, &player, sizeof(int));        // Player number
    write(fd, &symbol2, sizeof(char));      // Symbol
    write(fd, &rows, sizeof(int));          // Length of table
    write(fd, &columns, sizeof(int));       // Height of table
}

/// @brief Function to get the Clients PID
void getClient() {
    // Create and initialize structure to contain PIDs
    struct shmid_ds buf;
    shmctl(shmid, IPC_STAT, &buf);

    // Get Player 1
    semOp(semid, 0, -1);    // P(mutex)
    shmctl(shmid, IPC_STAT, &buf);  
    players.pid1 = buf.shm_lpid;
    printf("Waiting for another Client\n");

    // 1 player and 1 bot
    if(addr[rows*columns] == '9'){
        printf("BOT\n");
        isBot = true;
        pidBot = fork();
        players.pid2 = pidBot;
    }
    else {  // Match with two real player
        // Get Player 2
        semOp(semid, 0, -1);    // P(mutex)
        shmctl(shmid, IPC_STAT, &buf);
        players.pid2 = buf.shm_lpid;
    }

    if(pidBot != 0) {   // I'm the Server
        printf("PID1: %d \nPID2: %d\n", players.pid1, players.pid2);
        printf("##### STARTING THE GAME! #####\n");
    }
}

/// @brief Function that check if a Client has won by row, column or diagonal
void checkWin(){
    int countP1;
    int countP2;
    char character;
    
    // Check rows
    for(int i = 0; i < rows; i++) {
        // Reset counter
        countP1 = 0;
        countP2 = 0;

        for(int j = 0; j < columns && countP1 != 4 && countP2 != 4; j++) {
            character = addr[j + (i*columns)];
            if(character == players.sign1) {
                countP1++;
                countP2 = 0;
            }
            else if(character == players.sign2) {
                countP2++;
                countP1 = 0;
            }
            else{
                countP1 = 0;
                countP2 = 0;
            }
        }

        // Check if someone has won
        if(countP1 == 4)
            sendStatus(1);
        else if(countP2 == 4)
            sendStatus(2);
    }

    // Check columns
    for(int j = 0; j < columns; j++) {
        // Reset counter
        countP1 = 0;
        countP2 = 0;
        
        for(int i = 0; i < rows && countP1 != 4 && countP2 != 4; i++) {
            character = addr[j + (i*columns)];
            if(character == players.sign1) {
                countP1++;
                countP2 = 0;
            }
            else if(character == players.sign2) {
                countP2++;
                countP1 = 0;
            }
            else{
                countP1 = 0;
                countP2 = 0;
            }
        }

        // Check if someone has won
        if(countP1 == 4)
            sendStatus(1);
        else if(countP2 == 4)
            sendStatus(2);
    }

    // Check diagonals
    // Front diagonal
    char prec = players.sign1;
    bool endFirst = false;

    for(int j = 0, row = rows - 1, count = 0; j < (columns - 3) && row >= 3 && count != 4; j++, count = 0) {
        for(int y = row, x = j; y >= 0 && x < columns && count != 4; y--, x++) {
            character = addr[x + (y*columns)];
            
            if(character == prec) 
                count++;
            else if (character != '\0') {
                prec = character;
                count = 1;
            }
            else
                count = 0;
        }

        if(count == 4) {
            if(character == players.sign1)
                sendStatus(1);
            else
                sendStatus(2);
        }
        
        // If i'm the last column
        if(j == columns - 4)
            endFirst = true;    // Go to the line above
        if(endFirst) {    // If I check other rows (beyond the last one) I keep the first column
            j = -1;
            row--;
        }        
    }

    // Back diagonal
    prec = players.sign1;
    endFirst = false;
    for(int j = columns - 1, row = rows - 1, count = 0; j >= 3 && row >= 3 && count != 4; j--, count = 0) {
        for(int y = row, x = j; y >= 0 && x >= 0 && count != 4; y--, x--) {
            character = addr[x + (y*columns)];
            
            if(character == prec) 
                count++;
            else if (character != '\0') {
                prec = character;
                count = 1;
            }
            else
                count = 0;
        }

        if(count == 4) {
            if(character == players.sign1)
                sendStatus(1);
            else
                sendStatus(2);
        }
        
        // If i'm the last column
        if(j == 3)
            endFirst = true;    // Go to the line above
        if(endFirst) {    // If I check other rows (beyond the last one) I keep the first column
            j = columns;
            row--;
        }
    }

    // Check for spare
    bool spare = true;
    
    for(int i = 0; i < rows && spare; i++) {
        if(addr[i] == '\0')
            spare = false;
    }

    if(spare)
        sendStatus(0);
}

/// @brief Function that send a signal to the Clients to tell if someone has won, after that close the game
/// @param result who win, lose or spare the match
void sendStatus(int result) {
    if(result == 1){        //P1 WIN
        addr[rows*columns] = '1';
        kill(players.pid1, SIGUSR1);
        
        if(!isBot)  // There is no bot
            kill(players.pid2, SIGUSR1);
    }
        
    if(result == 0){        //SPARE
        addr[rows*columns] = '0';
        kill(players.pid1, SIGUSR1);
        
        if(!isBot)  // There is no bot
            kill(players.pid2, SIGUSR1);
    }
        
    if(result == 2){       //P2 WIN
        addr[rows*columns] = '2';
        kill(players.pid1, SIGUSR1);
        
        if(!isBot)  // There is no bot
            kill(players.pid2, SIGUSR1);
    }

    // Check if bot exists
    if(isBot)
        kill(players.pid2, SIGTERM);

    // Close all the shared things
    if(!closeAll(addr, shmid))
        exit(1);
    
    printf("Closing terminated successfully!\n");
    exit(0);
}

/// @brief Close all semaphores and shared memory
/// @param addr address of shared memory to detach
/// @param shmid shared memory id
/// @return true if close correctly otherwhise false
bool closeAll(char *addr, int shmid) {
    // Remove shared memory
    remove_shared_memory(shmid);
    
    // Close FIFO
    unlink("/tmp/share");

    // Remove the created semaphore set
    if (semctl(semid, 0, IPC_RMID, NULL) == -1)
        errExit("semctl IPC_RMID failed");

    if(close(fd) == -1)
        errExit("Error closin FIFO file!\n");
        
    return true;
}

/// @brief Funcion to handle SIGINT signal
/// @param sig signal passed
void sigHandler(int sig) {
    count_sig++;

    if(count_sig == 1)
        printf("\nPress Ctrl-C another time to close all!\n");
    else {
        printf("\nThe signal Ctrl-C was pressed for the 2th time!\nClosing in progress...\n");

        printf("PID 1: %d\nPID 2: %d\n", players.pid1, players.pid2);
        
        // Check if the clients exists before closing them
        if(players.pid1 != 0)
            kill(players.pid1, SIGUSR2);
        if(players.pid2 != 0 && !isBot)
            kill(players.pid2, SIGUSR2);

        // Check if bot exists
        if(isBot)
            kill(players.pid2, SIGTERM);

        // Check if son exists
        if(pid != 0)
            kill(pid, SIGTERM);
        
        // Close all the shared things
        if(!closeAll(addr, shmid))
            exit(1);
        
        printf("Closing terminated successfully!\n");
        exit(0);
    }
}

/// @brief Function to handle SIGUSR1 (Client quitted)
/// @param sig signal passed
void sigHandlerUserOne(int sig){

    if(addr[rows*columns] == '2') {     // Tell the Client that the other has quitted
        addr[rows*columns] = '2';
        printf("Player 1 has quitted!\n");
        
        // Check if player 2 exists
        if(players.pid2 != 0 && !isBot)
            kill(players.pid2, SIGUSR1);
    }
    else {
        addr[rows*columns] = '3';       // Tell the Client that the other has quitted
        printf("Player 2 user has quitted!\n");
        
        // Check if player 1 exists
        if(players.pid1 != 0)
            kill(players.pid1, SIGUSR1);
    }

    // Check if bot exists
    if(isBot)
        kill(players.pid2, SIGTERM);

    // Check if son exists
    if(pid != 0)
        kill(pid, SIGTERM);
        
    // Close all the shared things
    if(!closeAll(addr, shmid))
        exit(1);
    
    printf("Closing terminated successfully!\n");
    exit(0);
}

/// @brief Function to handle SIGALARM (Move timeout)
/// @param sig signal passed
void sigHandlerTimeOut(int sig){

    if(addr[rows*columns] == '4') {     // If player 2 waited too much
        printf("Player 2 waited too much!\n");
        kill(players.pid1, SIGUSR1);
        
        if(!isBot)  // There is no bot
            kill(players.pid2, SIGUSR1);
    }
    else {                              // If player 1 waited too much
        printf("Player 1 waited too much!\n");
        kill(players.pid1, SIGUSR1);

        if(!isBot)  // There is no bot
            kill(players.pid2, SIGUSR1);    
    }

    // Check if bot exists
    if(isBot)
        kill(players.pid2, SIGTERM);

    // Check if son exists
    if(pid != 0)
        kill(pid, SIGTERM);

    // Close all the shared things
    if(!closeAll(addr, shmid))
        exit(1);
    
    printf("Closing terminated successfully!\n");
    exit(0);
}
