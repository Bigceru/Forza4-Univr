/******************
* VR471650
* Davide Cerullo
* VR472656
* Edoardo Bazzotti
* 08/05/2023
******************/
#include <sys/shm.h>
#include <sys/stat.h>

#include "errExit.h"
#include "shared_memory.h"

int alloc_shared_memory(key_t shmKey, size_t size) {
    // get, or create, a shared memory segment
    int shmid = shmget(shmKey, size, IPC_CREAT | S_IWUSR | S_IRUSR);
    if (shmid == -1)
        errExit("shmget failed");
    return shmid;
}

void *get_shared_memory(int shmid, int shmflg) {
    // attach the shared memory
    int *address = (int *)shmat(shmid, NULL, shmflg);
    
    if(address == (void *) -1) {
        errExit("Error attaching memory segment!\n");
    } 

    return address;
}

void free_shared_memory(void *ptr_sh) {
    // detach the shared memory segments
    if(shmdt(ptr_sh) == -1)
        errExit("Unable to detach the shared memory segment!\n");
}

void remove_shared_memory(int shmid) {
    // delete the shared memory segment
    if(shmctl(shmid, IPC_RMID, NULL) == -1)
        errExit("Unable to delete the shared memory segment!\n");
}