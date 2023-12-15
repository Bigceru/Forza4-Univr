/******************
* VR471650
* Davide Cerullo
* VR472656
* Edoardo Bazzotti
* 08/05/2023
******************/
#include "shared_memory.h"
#include "errExit.h"
#include "semaphore.h"

#include <stdio.h>
#include <signal.h>  // Signal
#include <stdbool.h> // Bool
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h> // Shared memory
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

/* Define global variables */
int shmid;          // Shared memory's ID
int semid;          // Semaphore's ID
char *addr;         // Shared memory as large as the matrix + char
int fd;             // FIFO
int rows, columns;  // Dimensions of the matrix

// Struct to rappresent a player
typedef struct {
    int number;
    const char *name[255];
    char symbol;
} t_player;
t_player player;

/* Define methods */
void readFifo();
void showGame();
bool insertPlay(int play);
void sigHandler(int sig);
void sigHandlerUserOne(int sig);
void sigHandlerUserTwo(int sig);
bool closeAll(char *addr);

int main(int argc, char const *argv[])
{
    /* Print error if the argument is incorrect*/
    if((argc < 2 || argc > 3) || (argc == 3 && (strlen(argv[2]) != 1 || argv[2][0] != '*'))) {
        printf("Singleplayer usage:%s myName \nMultiplayer usage:%s myName \"*\"\n", argv[0], argv[0]);
        return 1;
    }

    *player.name = argv[1];     // Get player name
    
    // Change sigHandler for SIGINT --> Ctrl-C
    if (signal(SIGINT, sigHandler) == SIG_ERR)
    {
        printf("change signal handler failed");
    }

    // Change sigHandler for SIGUSR1 --> Win, Lose, Spare
    if (signal(SIGUSR1, sigHandlerUserOne) == SIG_ERR)
    {
        printf("change signal handler failed");
    }

    // Change sigHandler for SIGUSR2 --> Server terminated
    if (signal(SIGUSR2, sigHandlerUserTwo) == SIG_ERR)
    {
        printf("change signal handler failed");
    }

    // Change sigHandler for SIGHUP --> Client terminated by closing terminal
    if (signal(SIGHUP, sigHandler) == SIG_ERR)
    {
        printf("change signal handler failed");
    }

    readFifo();     // Read FIFO information

    // Attach segment to memory address space of the process
    addr = (char *)get_shared_memory(shmid, 0);

    // Check if the game is: 1 Client 1 Bot
    if(argc == 3)
        addr[rows*columns] = '9';   // Tell the Server there is a bot

    if(player.number == 1)      // Notify the wait for another client to start
        printf("Waiting for another Client\n");
    else
        printf("Waiting the other player move...\n");
        
    // Print Client info
    printf("I'm player %d --> %s\n", player.number, *player.name);

    semOp(semid, 0, 1);     // V(mutex)
    
    int play = 0;   // Selected column
    
    while (1) {
        semOp(semid, player.number, -1);        // P(giocatore)

        // It's my turn
        printf("It's your turn %s (%c), you have 30s to move\n", *player.name, player.symbol);
        showGame();
        
        // Check if the play is valid
        do {
            printf("Choose the column: ");
            
            // Check if user insert an integer
            if(!scanf("%d", &play)) {
                scanf("%*[^\n]");       //discard that line up to the newline (%[^\n] would take all characters in a single line as input, the * don't save the character)
                printf("Could not read an integer value!\n");
                play = -1;
            }
        } while(play < 0 || play >= columns || !insertPlay(play));
        
        // Show the game
        showGame();
        printf("Waiting the other player...\n");   

        semOp(semid, 0, 1);                     // V(mutex)
    }
    return 0;
}

/// @brief Function to read values from FIFO
void readFifo()
{
    // Open  FIFO file
    fd = open("/tmp/share", O_RDONLY);
    if (fd == -1)
        errExit("Error opening FIFO\n");

    // Read Fifo values
    read(fd, &shmid, sizeof(int));
    read(fd, &semid, sizeof(int));
    read(fd, &player.number, sizeof(int));
    read(fd, &player.symbol, sizeof(char));
    read(fd, &rows, sizeof(int));
    read(fd, &columns, sizeof(int));
}

/// @brief Function to insert the play in the game
/// @param play column to insert coin
/// @return boolean if the play has gone (true) or not (false)
bool insertPlay(int play) {
    // Check if the column selected is full
    if(addr[play] != '\0') {
        printf("The selected column is full!\n");
        return false;
    }
    
    // Cicle the matrix
    for(int i = rows-1; i >= 0; i--){
        if(addr[play + (i * columns)] == '\0') {    // Search the space to insert the coin
            addr[play + (i * columns)] = player.symbol;
            printf("Move done in %dx%d\n", i, play);
            return true;
        }
    }

    return false;
}

/// @brief Function showing the game table
void showGame() {
    // Cicle the matrix for the print
    for (int i = 0; i < rows; i++){
        for (int j = 0; j < columns; j++){
            if(j == 0)
                printf("|");

            if(addr[j + (i*columns)] == '\0')
                printf("   |"); 
            else
                printf(" %c |", addr[j + (i*columns)]);
        }
        
        printf("\n");
        for(int x = 0; x < columns; x++){
            printf("----");
        }
        printf("-\n");
    }
    
    // Print column counter
    for(int i = 0; i < columns; i++){
        printf("  %d ",i);
    }
    printf("\n");
}


/// @brief Close all semaphores and shared memory
/// @param addr address of shared memory to detach
/// @return true if close correctly otherwhise false
bool closeAll(char *addr)
{
    // Close shared memory and segment
    free_shared_memory(addr); // Detach shared memory

    if (close(fd) == -1)
        errExit("Error: fd close");

    return true;
}

/// @brief Funcion to handle SIGINT signal (Ctrl-C)
/// @param sig signal passed
void sigHandler(int sig)
{   
    struct shmid_ds buf;

    // Taking server PID
    if(shmctl(shmid, IPC_STAT, &buf) == -1)
        errExit("Error during shmctl");
        
    pid_t server = buf.shm_cpid;

    if(player.number == 1)
        addr[rows*columns] = '2';
    else
        addr[rows*columns] = '1';
        
    printf("\nYOU LOSE\n");
    kill(server, SIGUSR1);
    if (!closeAll(addr))
        exit(1);
        
    printf("\nClosing terminated successfully!\n");
    exit(0);
}

/// @brief Funcion to handle SIGUSR1 signal (Game terminated / Client quitted / Timeout move)
/// @param sig signal passed
void sigHandlerUserOne(int sig)
{
    printf("\nGame terminated: ");

    switch (addr[rows*columns])
    {
        case '0':   //SPARE
            printf("SPARE!\n");
            break;
        case '1':   //WIN OR LOSE
            if(player.number == 1)
                printf("YOU WIN!\n");
            else
                printf("YOU LOSE!\n");
            break;
        case '2':   //WIN OR LOSE
            if(player.number == 2)
                printf("YOU WIN!\n");
            else
                printf("YOU LOSE!\n");
            break;
        case '3':   // The other Client has quitted
            printf("YOU WIN! \nThe other user has quitted!\n");
            break;
        case '4':   // Client 2 has waited too much for the move
            if(player.number == 1)
                printf("YOU WIN! \nThe other user waited too much for the move!\n");
            else
                printf("YOU LOSE! You waited too much for the move!\n");
            break;
        case '5':   // Client 1 has waited too much for the move
            if(player.number == 2)
                printf("YOU WIN! \nThe other user waited too much for the move!\n");
            else
                printf("YOU LOSE! You waited too much for the move!\n");
            break;
        default:
            break;
    }

    printf("\n");
    showGame();

    // Close all the shared things
    if (!closeAll(addr))
        exit(1);

    printf("\nClosing terminated successfully!\n");
    exit(0);
}

/// @brief Funcion to handle SIGUSR2 signal (Server terminated)
/// @param sig signal passed
void sigHandlerUserTwo(int sig){
    printf("\nThe server is terminated, so the match must end!\n");

    // Close all the shared things
    if (!closeAll(addr))
        exit(1);

    printf("\nClosing terminated successfully!\n");
    exit(0);
}