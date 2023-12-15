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
bool insertPlay(int play);

int main() {
    *player.name = "BOT";     // Get player name

    readFifo();     // Read FIFO information

    // Attach segment to memory address space of the process
    addr = (char *)get_shared_memory(shmid, 0);

    printf("Bot connected!\n");

    int play = 0;   // Selected column

    while(1) {
        semOp(semid, 2, -1);            // P(giocatore2)

        do {
            play = rand()%(columns+1);
        } while(play < 0 || play >= columns || !insertPlay(play));

        semOp(semid, 0, 1);             // V(mutex)
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
        return false;
    }
    
    // Cicle the matrix
    for(int i = rows-1; i >= 0; i--){
        if(addr[play + (i * columns)] == '\0') {    // Search the space to insert the coin
            addr[play + (i * columns)] = player.symbol;
            return true;
        }
    }

    return false;
}