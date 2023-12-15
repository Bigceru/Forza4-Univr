/******************
* Davide Cerullo
* Edoardo Bazzotti
* 08/05/2023
******************/
#include <sys/sem.h>

#include "semaphore.h"
#include "errExit.h"

void semOp (int semid, unsigned short sem_num, short sem_op) {
    struct sembuf sop = {sem_num, sem_op};

    while(semop(semid, &sop, 1) == -1){}    // If the semaphore failed, redo it
        //errExit("semop failed");
}
