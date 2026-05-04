#ifndef INITIALIZATION_H
#define INITIALIZATION_H

// This file contains the declaration of the hiss_init function, which initializes the Hisstoria environment by creating a .hiss folder if it doesn't exist. It also includes a check_hiss function to verify the existence of the .hiss folder and handle initialization accordingly.
int hiss_init(void);

int check_folder_hiss(void);

#endif /* INITIALIZATION_H */
