/*
    * initialization.c
    * Hisstoria initialization and setup functions to prepare the environment of hiss folder and config file.
    * This file is responsible for creating the necessary directory structure and configuration files for Hisstoria
*/

// HEADER
#include "initialization.h"
#include "os.h"
#include <stdio.h>
#include <stdbool.h>
#include <xkeycheck.h>
/* ------------------------------------------------------------ */
const char* INITIAL_CONTENT =
    "# Hisstoria Configuration File\n"
    "# This file is used to store configuration settings for Hisstoria.\n";

// Initializes the Hisstoria environment by creating a .hiss folder if it doesn't exist.
int hiss_init(void) {
    bool status_hiss_exist = check_folder_hiss();
    bool start_init = false;

    bool OverallStatus = true;


    if (status_hiss_exist == 0 && !start_init == OverallStatus) {
        fprintf(stderr, "[hiss]: initialization successful\n");
        return 0;
    } else {
        fprintf(stderr, "[hiss]: initialization failed\n");
        return 1;
    }
    
}

int check_folder_hiss(void)   {
    bool isDone = false;

    // Check if the .hiss folder already exists
    if(cmd_ck(".hiss") == 0){
        fprintf(stderr, "[hiss]:  .hiss folder already exists\n");
        return 1;
    } else if (cmd_ck(".hiss") != 0) {
        // Create the .hiss folder
        cmd_mkdir(".hiss");

        //double check if the folder was created successfully
        if(cmd_ck(".hiss") == 0){
            /*
            Create a configuration file inside the .hiss folder with some default content.
            This file can be used to store user preferences and settings for Hisstoria.
            */

            // Create the .HISS config file and write the initial content to it
            cmd_mkfile(".hiss/.HISS");
            char *args[] = {(char *)INITIAL_CONTENT};
            cmd_write(".hiss/.HISS", 1, args);

            // If we reach this point, the initialization was successful
            isDone = true;
        } else {
            fprintf(stderr, "[hiss]:  failed to create .hiss folder\n");
            return 1;
        }
    }
    return isDone ? 0 : 1;
}