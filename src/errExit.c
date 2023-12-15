/******************
* Davide Cerullo
* Edoardo Bazzotti
* 08/05/2023
******************/
#include "errExit.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

void errExit(const char *msg){
    perror(msg);

    // If the error isn't made by interrupted system call
    // if(errno != EINTR)
    exit(1);
}

