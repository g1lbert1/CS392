/**
 * Tristan Mikiewicz
 * I pledge my honor that I have abided by the Stevens Honor System
 * CS392 HW4
 * */
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"
#define MAX_CHAR 1024
#define MAX_ARG 100

volatile sig_atomic_t interrupted = 0;

void signal_handler(int SIG){
    interrupted = 1;
}
//parsing input from user, tokenizing each individual input
void parseArgs(char *user_input, char** args){
    int count = 0;
    char *arg = strtok(user_input, " \t\n"); //space, tab, newline
    while(count < MAX_ARG - 1 && arg != NULL){
        args[count++] = arg;
        arg = strtok(NULL, " \t\n");
    }
    //last elem has to be null for exec:
    args[count] = NULL;
}

int main(int argc, char** argv){
    //signal handler for cntrl+C 
    struct sigaction sa = {0};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    if(sigaction(SIGINT, &sa, NULL) == -1){
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
    }
    if(sigaction(SIGCHLD, &sa, NULL) == -1){
        fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
    }
    char* arguments[MAX_ARG];
    char input[MAX_CHAR];
    uid_t uid = getuid();
    //main loop
    while(1){
        if(interrupted){
            interrupted = 0;
            continue;
        }
        //printing the prompt of the shell
        char cwd[MAX_CHAR];
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("%s[%s]> %s", BLUE, cwd, DEFAULT);
        }else{
            fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
            continue;
        }
        //reading input from user
        if(fgets(input, MAX_CHAR, stdin) == NULL){
            if(errno == EINTR){
                clearerr(stdin);
                continue;
            }
            fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
            continue;
        }
        parseArgs(input, arguments); 
        //cd command
        if(strcmp(arguments[0], "cd") == 0){
            //simply cd or cd ~, taking you to the home directory
            if(arguments[1] == NULL || (strcmp(arguments[1], "~") == 0 && arguments[2] == NULL)){
                struct passwd *pass = getpwuid(uid);

                if(pass == NULL){
                    fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
                    continue;
                }
                if(chdir(pass->pw_dir) != 0){
                    fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", pass->pw_dir, strerror(errno));
                    continue;
                }
            }else{
                if(arguments[2] != NULL){
                    fprintf(stderr, "Error: Too many arguments to cd.\n");
                    continue;
                }
                if(chdir(arguments[1]) != 0){
                    fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", arguments[1], strerror(errno));
                    continue;
                    
                }
            }
            continue;
        }
    
        //exit command
        if(strcmp(arguments[0], "exit") == 0){
            exit(EXIT_SUCCESS);
        }
        //pwd command
        if(strcmp(arguments[0], "pwd") == 0){
            printf("%s\n", cwd);
            continue;
        }
        //lf command
        if(strcmp(arguments[0], "lf") == 0){
            DIR *dp = opendir(cwd);

            if(dp == NULL){
                fprintf(stderr, "Error: Cannot open %s. %s.\n", cwd, strerror(errno)); //opendir failed
            }else{
                struct dirent *dirp;

                while((dirp = readdir(dp)) != NULL){
                    if(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0){
                        continue; //avoid loop
                    }
                    struct stat fileinfo;
                    int status = stat(dirp->d_name, &fileinfo);
                    if(status){
                        fprintf(stderr, "Error: Stat failed. %s\n", strerror(errno));
                    }
                    if(S_ISREG(fileinfo.st_mode)){
                        printf("%s\n", dirp->d_name);
                    }
                }
                continue;
            }
        }
        //lp command
        if(strcmp(arguments[0], "lp") == 0){
            DIR *dp = opendir("/proc");
            if(dp == NULL){
                fprintf(stderr, "Error: Cannot open /proc. %s.\n", strerror(errno)); 
            }else{
                struct dirent *dirp;
                while((dirp = readdir(dp)) != NULL){
                    if(dirp->d_type == DT_DIR && isdigit(dirp->d_name[0])){
                        pid_t pid = atoi(dirp->d_name);
                        struct passwd *pass;
                        struct stat fileinfo;
                        FILE *fp;
                        char path[MAX_CHAR];
                        char cmd[MAX_CHAR];
                        snprintf(path, sizeof(path), "/proc/%s/cmdline", dirp->d_name);
                        int status = stat(path, &fileinfo);
                        if(status){
                            fprintf(stderr, "Error: Stat failed. %s.\n", strerror(errno));
                            continue;
                        }
                        pass = getpwuid(fileinfo.st_uid);
                        fp = fopen(path, "r");
                        if(!fp){
                            fprintf(stderr, "Error: Failed to open file."); 
                            continue;
                        }
                        if(fgets(cmd, sizeof(cmd), fp) == NULL){
                            fprintf(stderr, "Error: Failed to read from file %s\n", path);
                            continue;
                        }
                        printf("%d %s %s\n", pid, (pass) ? pass->pw_name : "unknown", cmd);
                        
                    }
                }
                closedir(dp);
            }

            
        }else{
            pid_t pid = fork();
            int status;
            if(pid < 0){
                fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
            }
            if(pid == 0){
                if(execvp(arguments[0], arguments) == -1){
                fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
                exit(EXIT_FAILURE);
                }
            }
            else{
                if(waitpid(pid, &status, 0) == -1) {
                    if(errno == EINTR && interrupted){
                        interrupted = 0;
                    }
                    else
                        fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));    
                }else if(WIFSIGNALED(status)){
                    continue;
                }
            }   
        }
    }
}
    













