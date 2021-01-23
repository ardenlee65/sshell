//Arden Lee
//Raj Jagannath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>


#define CMDLINE_MAX 512
#define NUMPIPES_MAX 10
#define NUMVARS_MAX 26

struct Command {
    char fullLine[CMDLINE_MAX];
    char **args;
    int numCommands;
    int numPipes;
};

void addSpace(char* line, int pos){
    //also moves the char on position pos
    int length = (int)strlen(line);
    for (int currentPos = length; currentPos >= pos; currentPos--){
        line[currentPos]=line[currentPos - 1];
    }
    line[pos]= ' ';
}

void addSpaceAround(char* line, char targetChar){ //replaces any ">" or "|" with " > " and " | "
    int length = (int)strlen(line);
    for (int pos = 0; pos < length; pos++){
        if (line[pos] == targetChar){
            addSpace(line, pos);
            addSpace(line, pos + 2);
            pos+=2;
        }
    }
}

void shiftLeft(char* line){ //used to remove space at the beginning
    int length = (int)strlen(line);
    for (int pos = 0; pos < length - 1; pos++){
        line[pos] = line[pos+1];
    }
    line[length - 1] = '\0';
}

char*** splitByPipe(struct Command instruction){
    //echo hello world | grep hello | grep world
    //answer = {{echo, hello, world, NULL}, {grep, hello, NULL}, {grep, world, NULL}}
    char*** answer = (char***)malloc(sizeof(char**)*(instruction.numPipes + 2));
    for (int mallocPos = 0; mallocPos <= instruction.numPipes; mallocPos++){
        answer[mallocPos] = (char**)malloc(sizeof(char*));
    }
    int wordCount = 0;
    int arrayNum = 0;
    for (int cur = 0; cur < instruction.numCommands; cur++){
        if (strcmp(instruction.args[cur], "|") == 0){
            //finding a pipe means we start a new array
            answer[arrayNum][wordCount] = NULL;
            arrayNum++;
            wordCount = 0;
        } else{
            answer[arrayNum][wordCount++] = instruction.args[cur];
            answer[arrayNum] = (char**)realloc(answer[arrayNum], sizeof(char*)*(wordCount + 1));
        }
    }
    //needs to be NULL terminated
    answer[arrayNum][wordCount] = NULL;
    answer[instruction.numPipes + 1] = NULL;
    return answer;
}
void pipingFunct(struct Command instruction, int* writeOut, char* exitCodes){ //assume there is no '>'
    char*** instructionArray = splitByPipe(instruction);
    int fd[2];
    int curPipeNum = 0; //used to save the exit codes, youngest child has the greatest value
    pid_t pipePID;
    for (int pos = instruction.numCommands - 1; pos >= 0; pos--) {
        if (strcmp(instruction.args[pos], "|") == 0) { //finds a pipe
            curPipeNum++;
            pipe(fd); //0 is read, 1 is write
            pipePID = fork();
            if (pipePID == 0) { //Child
                instruction.numPipes--;
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                continue;
            } else if (pipePID > 0) { //Parent
                int pipeStatus;
                waitpid(pipePID, &pipeStatus, 0);
                read(writeOut[0], exitCodes, sizeof(exitCodes));
                int x = WEXITSTATUS(pipeStatus);
                exitCodes[curPipeNum - 1] = (char)(x + '0');
                write(writeOut[1], exitCodes, sizeof(exitCodes));
                //write out
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                execvp(instructionArray[instruction.numPipes][0], instructionArray[instruction.numPipes]);
                perror("execvp");
                fprintf(stderr, "Error: command not found\n");
                exit(WEXITSTATUS(pipeStatus));
            }
        }
    }
    if (pipePID == 0){ //Child cont.
        //very first write, can't know exit code until after child exits
        write(writeOut[1], exitCodes, sizeof(exitCodes));
        execvp(instructionArray[instruction.numPipes][0], instructionArray[instruction.numPipes]);
        perror("execvp");
        fprintf(stderr, "Error: command not found\n");
        exit(1);
    }
}

void redirectFunct(struct Command instruction, int* errorOut, char* exitCodes) {
    char **redirectFrom = (char **) malloc(sizeof(char *)); //commands that we want to run, left of >
    for (int pos = 0; pos < instruction.numCommands; pos++) {
        redirectFrom[pos] = instruction.args[pos];
        redirectFrom = (char **) realloc(redirectFrom, sizeof(char *) * (pos + 2));
        //will not do any work until a ">" is found
        if (strcmp(instruction.args[pos], ">") == 0) {
            if (pos == 0){
                fprintf(stderr, "Error: missing command\n");
                exit(1);
            }
            redirectFrom[pos] = NULL;
            pid_t redirect = fork();
            if (redirect == 0) { //Child for redirect
                //need to check for out of bounds
                if ((pos + 1) >= instruction.numCommands) {
                    fprintf(stderr, "Error: no output file\n");
                    exit(1);
                }
                struct Command newInstruction; //modified instruction struct with no "> fileName"
                newInstruction.args = (char**)malloc(sizeof(char*));
                for (int position = 0; position < instruction.numCommands - 2; position++){
                    newInstruction.args[position] = instruction.args[position];
                    newInstruction.args = (char**)realloc(newInstruction.args, sizeof(char*)*(position + 2));
                }
                newInstruction.numCommands = instruction.numCommands - 2; //removal of ">" and "fileName"
                newInstruction.numPipes = instruction.numPipes;
                int fd = open(instruction.args[pos + 1], O_WRONLY | O_CREAT, 0744);
                dup2(fd, STDOUT_FILENO);
                //changes from STDOUT_FILENO to fd
                //call piping function
                //make new instance of instruction struct, but args without "> fileName"
                //change wordCount, don't care about fullLine
                pipingFunct(newInstruction, errorOut, exitCodes);
                execvp(redirectFrom[0], redirectFrom);
                FILE *source = fopen(instruction.args[pos - 1], "rw");
                FILE *dest = fopen(instruction.args[pos + 1], "rw");
                char line[CMDLINE_MAX];
                while (fgets(line, CMDLINE_MAX, source)) { //writes from one file into another, line by line
                    printf("%s", line);
                    fflush(stdout);
                }
                fclose(source);
                fclose(dest);
                close(fd);
                exit(0);
            } else if (redirect > 0) { //Parent for redirect
                int redStatus;
                FILE *clearFile = fopen(instruction.args[pos + 1], "w");
                fclose(clearFile); //clear contents
                waitpid(redirect, &redStatus, 0);
                if (instruction.numPipes > 0){
                    read(errorOut[0], exitCodes, sizeof(char)*NUMPIPES_MAX);
                    write(errorOut[1], exitCodes, sizeof(char)*NUMPIPES_MAX);
                }
                exit(WEXITSTATUS(redStatus));
            }
        }
    }
}

int main(void) {
    char cmd[CMDLINE_MAX];
    char* varList[NUMVARS_MAX];
    for (int var = 0; var < 26; var++){
        varList[var] = "";
    }
    while (1) {
        struct Command instruction;
        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);
        /* Get command line */
        fgets(cmd, CMDLINE_MAX, stdin);
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
            //fflush is to flush out the characters in the buffer
        }
        if (strcmp(cmd, "\n")==0){
            continue;
        }
        char cmdChangeable[CMDLINE_MAX]; //strtok changes spaces to NULL char, so we need another variable
        strcpy(instruction.fullLine, cmd);
        strcpy(cmdChangeable, instruction.fullLine);
        addSpaceAround(cmdChangeable, '>');
        addSpaceAround(cmdChangeable, '|');
        char* nullChar = strchr(instruction.fullLine, '\n');
        if (nullChar){ //don't care about things after the new line
            *nullChar = '\0';
        }
        nullChar = strchr(cmdChangeable, '\n');
        if (nullChar){ //don't care about things after the new line
            *nullChar = '\0';
        }
        int wordCount = 0;
        int shouldContinue = 0; //if a variable is used incorrectly
        char *word; //add word by word into instruction.args
        instruction.args = (char**)malloc(sizeof(char*));
        word = strtok(cmdChangeable, " \n");
        while (word != NULL) {
            instruction.args[wordCount] = strncat(word, "\0", 1);
            if (instruction.args[wordCount][0] == '$'){ //check if there is a "$" and ONLY a-z follows
                if (strlen(instruction.args[wordCount]) == 2 && (instruction.args[wordCount][1] >= 'a') &&
                    (instruction.args[wordCount][1] <= 'z')){
                    instruction.args[wordCount] = varList[instruction.args[wordCount][1] - 'a'];
                } else{
                    fprintf(stderr, "Error: invalid variable name\n");
                    shouldContinue = 1;
                    break;
                }
            }
            wordCount++;
            instruction.args = (char**)realloc(instruction.args, sizeof(char*)*(wordCount + 1));
            word = strtok(NULL, " \n");
        }
        if (shouldContinue){
            free(instruction.args);
            free(word);
            continue;
        }
        //needs to be NULL terminated
        instruction.args[wordCount] = NULL;
        instruction.numCommands = wordCount;
        instruction.numPipes = 0;
        for (int argPos = 0; argPos < instruction.numCommands; argPos++) { //Counts number of |
            if (strcmp(instruction.args[argPos], "|") == 0) {
                instruction.numPipes++;
            }
        }
        free(word);
        if (strcmp(instruction.args[0], "set")==0){ //handles setting variables
            if (instruction.numCommands > 2) {
                if (strlen(instruction.args[1]) == 1 && (instruction.args[1][0] >= 'a') &&
                    (instruction.args[1][0] <= 'z')) { //checks if variable is only one letter and is between a-z
                    char tempWord[CMDLINE_MAX];
                    strcpy(tempWord, instruction.args[2]);
                    varList[instruction.args[1][0] - 'a'] = tempWord;
                    fprintf(stderr, "+ completed '%s' [0]\n", instruction.fullLine);
                } else {
                    fprintf(stderr, "Error: invalid variable name\n+ completed '%s' [1]\n", instruction.fullLine);
                }
                free(instruction.args);
                continue;
            } else if (instruction.numCommands == 2){ //if we want to set var to ""
                if (strlen(instruction.args[1]) == 1 && (instruction.args[1][0] >= 'a') &&
                    (instruction.args[1][0] <= 'z')) {
                    varList[instruction.args[1][0] - 'a'] = "";
                    fprintf(stderr, "+ completed '%s' [0]\n", instruction.fullLine);
                    free(instruction.args);
                    continue;
                }
            } else{ //if no args after set
                fprintf(stderr, "Error: invalid variable name\n+ completed 'set' [1]\n");
                free(instruction.args);
                continue;
            }
        }
        if (instruction.numCommands > 16){ //checks for max number of arguments
            fprintf(stderr, "Error: too many process arguments\n");
            free(instruction.args);
            continue;
        }
        if ((strcmp(instruction.args[0], "|") == 0) || (strcmp(instruction.args[instruction.numCommands-1], "|") == 0) ||
            (strcmp(instruction.args[0], ">") == 0)){ //checks for missing args
            fprintf(stderr, "Error: missing command\n"); //if command starts or ends with ">" or "|"
            free(instruction.args);
            continue;
        } else if ((strcmp(instruction.args[instruction.numCommands-1], ">") == 0)){
            fprintf(stderr, "Error: no output file\n"); //if command starts or ends with ">" or "|"
            free(instruction.args);
            continue;
        }
        int foundRedir = 0;
        for (int check = 0; check < instruction.numCommands; check++){
            if (strcmp(instruction.args[check], ">") == 0){
                foundRedir = 1; //found >, if any | appear then mislocated output redirection
            } else if((strcmp(instruction.args[check], "|") == 0) && foundRedir){
                foundRedir = 2; //if pipe appears after a ">", it is undefined behavior
                fprintf(stderr, "Error: mislocated output redirection\n");
                free(instruction.args);
                break;
            }
        }
        if (foundRedir == 2){
            continue;
        }
        if ((instruction.numCommands > 2) && (strcmp(instruction.args[instruction.numCommands-2], ">") == 0)){ //if file has no permissions
            FILE *clearFile = fopen(instruction.args[instruction.numCommands-1], "w");
            if (!clearFile){
                fprintf(stderr, "Error: cannot open output file\n");
                free(instruction.args);
                continue;
            }
            fclose(clearFile); //clear contents
        }
        int currentPosition = 0;
        while (cmdChangeable[currentPosition] == ' '){
            shiftLeft(cmdChangeable);
        }
        char* nl = strchr(cmd, '\n');
        //finds first instance of '\n'
        if (nl)
            *nl = '\0';
        if (strcmp(cmdChangeable, "exit") == 0) { //make sure it works with spaces
            fprintf(stderr, "Bye...\n+ completed 'exit' [0]\n");
            break;
        }
        if (strcmp(instruction.args[0], "pwd") == 0){ //handles pwd
            char cwd[CMDLINE_MAX];
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
            fprintf(stderr, "+ completed '%s' [0]\n", instruction.fullLine);
            free(instruction.args);
            continue;
        }
        if (strcmp(instruction.args[0], "cd") == 0) { //handles cd
            if (chdir(instruction.args[1]) == -1) {
                fprintf(stderr, "Error: cannot cd into directory\n+ completed '%s' [1]\n", instruction.fullLine);
                free(instruction.args);
                continue;
            }
            fprintf(stderr, "+ completed '%s' [0]\n", cmd);
            free(instruction.args);
            continue;
        }
        pid_t pid;
        int errorOut[2];
        pipe(errorOut);
        char* exitCodes = (char*)malloc(sizeof(char*)*NUMPIPES_MAX);
        for (int null = 0; null < NUMPIPES_MAX; null++){
            exitCodes[null]='\0';
        } //exitCodes used for piping, to get children's exit codes
        pid = fork();
        if (pid == 0) { // Child
            redirectFunct(instruction, errorOut, exitCodes); //redir and piping do nothing if there is no ">" or "|"
            pipingFunct(instruction, errorOut, exitCodes);
            execvp(instruction.args[0], instruction.args);
            fprintf(stderr, "Error: command not found\n");
            exit(1);
        } else if (pid > 0) { // Parent
            int status;
            waitpid(pid, &status, 0);
            if (instruction.numPipes > 0){ //only read if there are pipes
                read(errorOut[0], exitCodes, sizeof(char)*NUMPIPES_MAX);
            }
            close(errorOut[0]); //no longer need to read
            close(errorOut[1]);
            if (instruction.numPipes > 0){ //Have pipes
                fprintf(stderr, "+ completed '%s' ", instruction.fullLine);
                for (int position = 0; position < instruction.numPipes; position++){
                    fprintf(stderr, "[%c]", exitCodes[position]);
                }
                fprintf(stderr, "[%d]", WEXITSTATUS(status));
                fprintf(stderr, "\n");
            } else{
                fprintf(stderr, "+ completed '%s' [%d]\n", instruction.fullLine, WEXITSTATUS(status));
            }
        } else {
            perror("fork");
            exit(1);
        }
        int num = instruction.numCommands;
        for (int x = 0; x < num; x++){
            instruction.args[x] = NULL;
        } //making sure we don't get trash values
        free(exitCodes);
        free(instruction.args);
        instruction.args = NULL;
    }
    return EXIT_SUCCESS;
}
