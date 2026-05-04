/*
    * initialization.c
    * Hisstoria initialization and setup functions to prepare the environment of hiss folder and config file.
    * This file is responsible for creating the necessary directory structure and configuration files for Hisstoria
*/

// HEADER
#include "initialization.h"
#include "os.h"
#include <stdio.h>

/* ------------------------------------------------------------ */

int hiss_init(void) {
    if(cmd_ck(".hiss") == 0){
        fprintf(stderr, "[hiss]:  .hiss folder already exists\n");
        return 1;
    } else if (cmd_ck(".hiss") == !0) {
        printf("[hiss]:  .hiss folder does not exist\n"); 
    }
    return 0;
}