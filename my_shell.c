#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main();
void printPrompt();
char* readLine();
char** parseLine(char*);
void changeDirectory(char*);
void externalCommand(char*,char**);
int backgroundCheck(char**);
int numberPipes(char**);
int executePiped(int, char*);
char** parseLineByPipe(char*);
char** parseLineByAmp(char*);


pid_t shell_pid;
#define MAX_LEN 256
#define MAX_ARGS 128
#define MAX_JOBS 12 //maximum amount of jobs in the background

int main(){

    shell_pid=getpid();
    pid_t toWaitFor[MAX_JOBS]; //deals with up to 10 jobs in the background
    int currentIndexPid=0; //current job in job backgorund list

    while (1){

        char** args;
        char* input;
        char inputForPipe[256];

        printPrompt();
        input=readLine();

        strcpy(inputForPipe,input);

        args=parseLine(input);

        if (args[0]){
          if  ( strcmp("cd",args[0]) == 0) {
            //Tries to change directory to entire arguement, and if arguement is not a directory itself tries to find currentworkingdirectory/arg
             char cwd[1024];
             getcwd(cwd,sizeof(cwd));
            int worked=chdir(args[1]);
            if ( worked != 0 ){
                int worked2=chdir(strcat(cwd,args[1]));
                if (worked2 != 0 ){
                    printf("Error with file directory %s\n",args[1]);}
                    return 0;
                }
        }
        else if (strcmp(args[0],"exit") ==0 || strcmp(args[0],"exit\n")==0 ){
            //Exit Command
              printf("Exiting Shell...\n");
                exit(0);

        }
        else if(strcmp(args[0],"jobs")==0){
            int i;
            for (i=0; i<MAX_JOBS-1;i++){
                //if (toWaitFor[i]){
                    printf("[%i] %i\n", i,toWaitFor[i]);

                //}
            }
        }
        else{ //args is not an internal command
            int background; //1 if should run in background, 0 otherwise
            int numPipes; //1 if involves piping, 0 otherwise

            background=backgroundCheck(args);

            numPipes=numberPipes(args);


            if ( numPipes==0 ){
                                //regular implementation- no piping
                //may include background (&)
                pid_t child;
                int status;
                child=fork();
                if (child<0) {
                    //error occured
                    fprintf(stderr,"Fork Failed\n");
                }
                else if (child!=0){
                    //Parent Process:
                    if (background==1){
                        //run in background
                      //   wait(child,&status,0);

                      toWaitFor[currentIndexPid]=waitpid(-1,&status,WNOHANG);
                      if (currentIndexPid>MAX_JOBS-1){
                          currentIndexPid=0;

                      }
                    }
                    else {
                        //Run Normal Way: foreground
                        wait(NULL);

                    }
                }
                else{
                    //Child Process:

                    if (background==1){
                        char** noBackgroundLine=parseLineByAmp(inputForPipe);
                        char** newArgs=parseLine(noBackgroundLine[0]);
                        execvp(newArgs[0],newArgs);
                    }
                    else{
                        execvp(args[0],args);
                    }

                    fprintf(stderr,"Error: %s command not found\n",args[0] );
                }
            }
            else{
                 //PIPING HERE: (cannot combine & and piping )
                executePiped(numPipes, inputForPipe);
            }

          }
      }
      free(input);
      free(args);
    }
}

int executePiped(int numberPipes, char* input ){

    char** splitPiped=parseLineByPipe(input);
    int i=0;
    int previous; //Prevous Output
    while (splitPiped[i]!= NULL){
        int pipes[2];// create new pipe each time
        pipe(pipes);
        int pid;
        char** command=parseLine(splitPiped[i]);
        pid=fork();
        if (pid< 0){
            fprintf(stderr, "Error with Fork.\n" );
        }
        else if (pid==0){
            //CHILD PROCESS
            if (i==0){//First process:
                dup2(0,STDIN_FILENO); //reversed from shell-pipe.c for loop
            }
            else{
                dup2(previous,STDIN_FILENO);
            }
            close(pipes[0]);

            if( splitPiped[i+1] != NULL){ //if there is an additional command after this, set up pipes
                dup2(pipes[1],STDOUT_FILENO); //set up to recieve
            }
            close(pipes[0]);
            execvp(command[0], command);
            printf("Error: command not found or invalid parameters. \n");
            exit(EXIT_FAILURE);
        }
        else{
            //PARENT PROCESS
            wait(NULL); //wait on child
            previous=pipes[0]; //passes the prevous stuff to next command
            close(pipes[1]);
        }
        i++;
        free(command);
    }
    return 0;

}
int backgroundCheck(char** array){
    //Tests to see if & contained in the array - 1 if true, 0 if false
    int i=0;
    int found=0;
    while (array[i]!= NULL){
        if (strcmp( array[i],"&")==0) found=1;
        i++;

    }
    return found;
}
int numberPipes(char** array){
    //Returns number of | in the array
    int i=0;
    int found;
    found=0;
    while (array[i]!= NULL){
        if (strcmp( array[i],"|")==0)
        {
            found++;
        }
        i++;
    }
    return found;

}

void externalCommand(char * command, char**args){
    //take args starting at command up until a & or | - background or pipe

    execvp(command,args);
    fprintf(stderr,"Error: %s command not found\n", command);

}

void printPrompt(){
    char cwd[1024];
    if ( getcwd(cwd,sizeof(cwd))!=NULL ) {
        printf("%s$ ",cwd);
    }
    else{
        printf("Directory Error$ ");
    }
}
char* readLine(){
        char *line=NULL;
        ssize_t bufsize=0;
        getline(&line,&bufsize,stdin);
        return line;

}
char** parseLine(char* line){//uses reccomended strtok function to split commands

    int bufsize=1024;
    char** tokens=malloc(bufsize*sizeof(char*)); //allocate the space for the tokens
    int position=0;
    char*p;
    p=strtok(line," \t\n");
    while(p!=NULL){
        tokens[position++]=p;
        p=strtok(NULL," \t\n");
    }
   // position++;
    tokens[position]=NULL;
    return tokens;

}
char** parseLineByPipe(char* line){
    int bufsize=1024;
    char** tokens=malloc(bufsize*sizeof(char*)); //allocate the space for the tokens
    int position=0;
    char*savePtr;
    char*p;

    p=strtok_r(line,"|", &savePtr);
    while(p!=NULL){
        tokens[position++]=p;
        p=strtok_r(NULL,"|", &savePtr );
    }
   // position++;
    tokens[position]=NULL;
    return tokens;
}
char** parseLineByAmp(char* line){
    int bufsize=1024;
    char** tokens=malloc(bufsize*sizeof(char*)); //allocate the space for the tokens
    int position=0;
    char*savePtr;
    char*p;

    p=strtok_r(line,"&", &savePtr);
    while(p!=NULL){
        tokens[position++]=p;
        p=strtok_r(NULL,"&", &savePtr );
    }
   // position++;
    tokens[position]=NULL;
    return tokens;
}
  
