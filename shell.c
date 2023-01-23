#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>

#define CAPACITY 50
#define TEN_CAPACITY 10
#define MAX_SIZE 1024
#define READ 0
#define WRITE 1

int i, pos, types_pos, argv_pos, retid, last_status, session, amper, 
    redirect, concatenate, last_session, to_error, wait_for_read,
    is_statement, argc2[CAPACITY];
    
char *token, 
     *outfile,
     *if_stat[TEN_CAPACITY][CAPACITY], 
     *then_stat[TEN_CAPACITY], 
     *else_stat[TEN_CAPACITY],

    text[TEN_CAPACITY],
    command[MAX_SIZE],
    prompt[TEN_CAPACITY],
    history[CAPACITY][CAPACITY],
    types[TEN_CAPACITY][CAPACITY],
    values[TEN_CAPACITY][CAPACITY],
    lastdir[MAX_SIZE],  // initialized to zero

    *argv1[TEN_CAPACITY][CAPACITY],
    *argv_temp[TEN_CAPACITY][CAPACITY];

int exec_cd(char *arg) {
    char curdir[MAX_SIZE];
    char path[MAX_SIZE];

    if (getcwd(curdir, sizeof curdir)) {
        /* current directory might be unreachable: not an error */
        *curdir = '\0';
    }
    if (arg == NULL) {
        arg = getenv("HOME");
    }
    if (!strcmp(arg, "-")) {
        if (*lastdir == '\0') {
            fprintf(stderr, "no previous directory\n");
            return 1;
        }
        arg = lastdir;
    } else {
        /* this should be done on all words during the parse phase */
        if (*arg == '~') {
            if (arg[1] == '/' || arg[1] == '\0') {
                snprintf(path, sizeof(path), "%s%s", getenv("HOME"), arg + 1);
                arg = path;
            } else {
                /* ~name should expand to the home directory of user with login `name` 
                   this can be implemented with getpwent() */
                fprintf(stderr, "syntax not supported: %s\n", arg);
                return 1;
            }
        }
    }
    if (chdir(arg)) {
        fprintf(stderr, "chdir: %s: %s\n", strerror(errno), path);
        return 1;
    }
    strcpy(lastdir, curdir);
    return 0;
}

void sighandler(int num) {
    write(STDOUT_FILENO,"You typed Control-C!\n",13);
}

int execute_commands(char *arguments[TEN_CAPACITY][CAPACITY]) {
    int fildes[2];
    int file;
    
    i = 0;
    while(i < argv_pos+1) {
        int status;
        pid_t pid;
        pipe(fildes);
        pid = fork();

        if(pid != 0) {
            wait(NULL);//wait for child to exit
            close(fildes[WRITE]);//close the writing end

            if(WIFEXITED(status)){

                if(WEXITSTATUS(status) == 0){
                    
                } else if(WEXITSTATUS(status) == 255) {
                    printf("The program %s does not exist\n", arguments[i][0]);
                    return 0;        

                } else {
                    printf("ERROR: Error code: %d\n", WEXITSTATUS(status));
                    return 0;
                }
            } else {
                printf("There was a problem that is not normal %d\n", status);
                return 0;
            }

        } else {

            if (redirect){
                file = open(outfile,O_WRONLY | O_CREAT, 0660); 
                close (STDOUT_FILENO) ; 
                dup(file); 
                close(file);     
       
            } else if (concatenate){
                file = open(outfile,O_WRONLY | O_CREAT | O_APPEND, 0600);
                close (STDOUT_FILENO); 
                dup2(file,STDOUT_FILENO);
                close(file);
    
            } else if (to_error) {
                file = open(outfile,O_RDWR | O_CREAT, 0600);
                close (STDOUT_FILENO); 
                dup2(file,STDERR_FILENO);
                close(file);
      
            } else {
                file = fildes[WRITE];
    
            } 
            
            if(file < 0)
                return 0;

            dup2(fildes[READ], STDIN_FILENO);
            if(i != argv_pos) 
                dup2(file, STDOUT_FILENO);
            close(fildes[READ]);

            if (last_session){
                last_status = execvp(argv_temp[i][0], argv_temp[i]);
                last_session = 0;
            } else {
                last_status = execvp(argv1[i][0], argv1[i]);
            }

            return 1;
        }

        i++;
    }

    return 1;
}

int main() {
    session = 1;
    pos = 0;
    types_pos = 0;
    argv_pos = 0;
    wait_for_read = 0;
    redirect = 0;
    concatenate = 0;
    amper = 0;
    is_statement = 0;
    strcpy(prompt, "hello");

    //fflush(stdin);
    //fflush(stdout);

    signal(SIGINT,sighandler);
    while (session) {
        printf("%s: ",prompt);
        fgets(command, MAX_SIZE, stdin);
        command[strlen(command) - 1] = '\0';
        if (command[0] == '\033') {
            int c,index = 0;
            system ("/bin/stty raw");
            while ((c=getchar()) != '.'){
                printf("\033[2J\033[1;1H");
                printf("%s",history[index]);
                c=getchar();
                 
                switch(c) { // the real value
                    case 27:
                        // code for arrow up
                        if(strcmp(history[index], "")){
                            index++;
                            index = index%pos;    
                        } 
                        break;

                    case 91:
                        // code for arrow down
                        if(strcmp(history[index], "")){
                            index--;
                            if(index < 0) index = 0;
                            index = index%pos;
                        }  
                        break;
                }   
            } 
            system ("/bin/stty cooked");
        }

        strcpy(history[pos++],command);
        pos = (pos)%CAPACITY;

        /* parse command line */
        i = 0;
        token = strtok (command," ");
        while (token != NULL) {
            argv1[argv_pos][i] = malloc(TEN_CAPACITY * sizeof argv1[0]);
            strcpy(argv1[argv_pos][i],token);
            token = strtok (NULL, " ");
            i++;
            if (token && ! strcmp(token, "|")) {
                argv1[argv_pos][i] = NULL;
                argc2[argv_pos] = i;
                i = 0;
                argv_pos++;
                token = strtok (NULL, " ");
            }

        }
        argv1[argv_pos][i] = NULL;
        argc2[argv_pos] = i;
        
        if(wait_for_read){
            strcpy(values[types_pos],argv1[argv_pos][0]);
            types_pos = (types_pos)%CAPACITY;
            types_pos++;
            wait_for_read = 0;
        }

        if(argv1[argv_pos][1] != NULL && argv1[argv_pos][0] != NULL && ! strcmp(argv1[argv_pos][1], "=") && ! strcmp(argv1[argv_pos][0], "prompt")){
            argv1[argv_pos][i - 2] = NULL;
             strcpy(prompt, argv1[argv_pos][i - 1]); 
        } 

        /* Is command empty */
        if (argv1[0] == NULL){
            continue;
        } else if(! strcmp(argv1[argv_pos][0], "quit")){
            session = 0;
            break;
        } else if(! strcmp(argv1[argv_pos][0], "!!")){
            if(pos > 1) last_session = 1;                  
        } else if(! strcmp(argv1[argv_pos][0], "cd")){
            exec_cd(argv1[argv_pos][1]);
        }else if(! strcmp(argv1[argv_pos][0], "echo")){
                strcpy(text,argv1[argv_pos][1]);
                if(text[0] == '$'){
                    for (size_t j = 0; j < types_pos+1; j++) {
                        if(! strcmp(types[j], text)){
                            argv1[argv_pos][1] = values[j];
                        }
                    }
                }
        } else if(! strcmp(argv1[argv_pos][0], "read")){
            wait_for_read = 1;
            strcpy(text,"$");
            strcat(text,argv1[argv_pos][1]);
            strcpy(types[types_pos],text);
        } else if(argv1[argv_pos][i-1] != NULL && ! strcmp(argv1[argv_pos][i-1], "fi") 
        && ! strcmp(argv1[0][0], "if")){
            size_t z = 1;
            size_t stat_pos = 0;
            size_t ind = 0;
            while(strcmp(argv1[ind][z], "then") != 0){
                if_stat[ind][stat_pos] = malloc(TEN_CAPACITY * CAPACITY * sizeof if_stat[0]);
                strcpy(text,argv1[ind][z]);
                strcpy(if_stat[ind][stat_pos],text);
                stat_pos++;
                z++;
                if (argv1[ind][z] == NULL){
                   ind++; 
                   z=0;
                   stat_pos = 0;
                }  
            }

            z++;
            stat_pos = 0;
            while(strcmp(argv1[ind][z], "else") != 0){
                then_stat[stat_pos] = malloc(TEN_CAPACITY * sizeof then_stat[0]);
                strcpy(text,argv1[ind][z]);
                strcpy(then_stat[stat_pos],text);
                stat_pos++;
                z++;
                if (argv1[ind][z] == NULL){
                   ind++; 
                   z=0;
                }
            }

            z++;
            stat_pos = 0;
            while(strcmp(argv1[ind][z], "fi") != 0){
                else_stat[stat_pos] = malloc(TEN_CAPACITY * sizeof else_stat[0]);
                strcpy(text,argv1[ind][z]);
                strcpy(else_stat[stat_pos],text);
                stat_pos++;
                z++;  
                if (argv1[ind][z] == NULL){
                   ind++; 
                   z=0;
                }
            }
            is_statement = 1;

        } else { 
            strcpy(text,argv1[argv_pos][0]);
            if(text[0] == '$'){
                strcpy(types[types_pos],argv1[argv_pos][0]);
                strcpy(values[types_pos],argv1[argv_pos][2]);
                types_pos = (types_pos)%CAPACITY;
                types_pos++;
            }
        }

        if (argv1[argv_pos][1] != NULL){
            if(! strcmp(argv1[argv_pos][1], "$?")){
                if(last_status == 1){
                    argv1[argv_pos][1] = "1";
                } else {
                    argv1[argv_pos][1] = "0";
                }
            } 
                
            if(argv_pos >= 0){
                for (size_t j = 0; j < argv_pos+1; j++) {
                    if (argc2[j] > 1 && ! strcmp(argv1[j][argc2[j] - 2], ">")) {
                        redirect = 1;
                        argv1[j][argc2[j] - 2] = NULL;
                        outfile = argv1[j][argc2[j] - 1];
                    } else if (argc2[j] > 1 && ! strcmp(argv1[j][argc2[j] - 2], ">>")){
                        concatenate = 1;
                        argv1[j][argc2[j] - 2] = NULL;
                        outfile = argv1[j][argc2[j] - 1];
                    } else if (argc2[j] > 1 && ! strcmp(argv1[j][argc2[j] - 2], "2>")){
                        to_error = 1;
                        argv1[j][argc2[j] - 2] = NULL;
                        outfile = argv1[j][argc2[j] - 1];
                    } else if (argc2[j] > 1 && ! strcmp(argv1[j][argc2[j] - 1], "&")) {
                        amper = 1;
                        argv1[argv_pos][argc2[j] - 1] = NULL;
                    }
                }
            } 
        }

        if(is_statement){
            last_status = execute_commands(if_stat);
            if(last_status)
                execvp(then_stat[0], then_stat);
            else 
                execvp(else_stat[0], else_stat);
        } else {
            execute_commands(argv1);
        }
        

        memset(argv_temp, 0,sizeof(argv_temp[argv_pos][i]) * CAPACITY * TEN_CAPACITY);//clear the arguments array

        for (size_t j = 0; j < argv_pos+1; j++) {
            for (size_t t = 0; t < argc2[j]; t++) {
                argv_temp[j][t] = malloc(TEN_CAPACITY * sizeof argv_temp[0]);
                if (concatenate && t == (argc2[j] - 2)) 
                    strcpy(argv_temp[j][t],">>");
                    
                else if(redirect && t == (argc2[j] - 2))
                  strcpy(argv_temp[j][t],">");

                else if(to_error && t == (argc2[j] - 2))
                    strcpy(argv_temp[j][t],"2>");

                else
                    strcpy(argv_temp[j][t],argv1[j][t]);
            } 
        }
        
        memset(argv1, 0,sizeof(argv1[argv_pos][i]) * CAPACITY * TEN_CAPACITY);//clear the arguments array
        argv_pos = 0;
        
    }
}