#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CMDLINE_MAX 512

int main(void)
{
    char cmd[CMDLINE_MAX];

    while (1) {
        char *nl;
        int retval;

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);
        char* word;
        char** args;
        args = (char**)malloc(sizeof(char*));
        int wordCount = 0;
        word = strtok(cmd, " ");
        while(word != NULL){
            args[wordCount++] = strncat(word, "\0", 1);
            args = (char**)realloc(args, sizeof(char*)*(wordCount+1));
            word = strtok(NULL, " \n");
        }
        free(word);

        /* Print command line if stdin is not provided by terminal */
        //isatty is if STDIN_FILENO is associated with the terminal
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
            //fflush is to flush out the characters in the buffer
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        //finds first instance of '\n'
        if (nl)
            *nl = '\0';

        /* Builtin command */
        if (!strcmp(cmd, "exit")) {
            fprintf(stderr, "Bye...\n");
            break;
        }

        /* Regular command */
        //retval = system(cmd);
        //fprintf(stdout, "Return status value for '%s': %d\n",
        //cmd, retval)
        /*if(fork() == 0){ // Child
            execlp()
        }*/

    }

    return EXIT_SUCCESS;
}
