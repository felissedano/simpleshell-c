#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>    //
#include <sys/stat.h> //
#include <setjmp.h>   //
#include <sys/wait.h> //

/* maximum number of args in a command */
#define LENGTH 20

// only terminate current foreground process when received CTRLC
void handle_sigint(int signal)
{
    // no need of any code
}
// collect defunc processes
void handle_sigchild(int signal)
{
    pid_t pid;
    int status;
    pid = waitpid(-1, &status, WNOHANG);
}

/* Parses the user command in char *args[].
   Returns the number of entries in the array */

int getcmd(char *prompt, char *args[], int *background)
{

    int length, flag, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    // check fof ionvalid command
    if (length == 0)
    {
        return 0;
    }
    // check for CTRL + D
    else if (length == -1)
    {
        exit(0);
    }

    /* check if background is specified */
    if ((loc = index(line, '&')) != NULL)
    {
        *background = 1;
        *loc = ' ';
    }
    else
    {
        *background = 0;
    }

    // Clear args
    memset(args, '\0', LENGTH);

    // Splitting the command and putting the tokens inside args[]
    while ((token = strsep(&line, " \t\n")) != NULL)
    {
        for (int j = 0; j < strlen(token); j++)
        {
            if (token[j] <= 32)
            {
                token[j] = '\0';
            }
            // Here you can add the logic to detect piping or redirection in the command and set the flags
        }
        if (strlen(token) > 0)
        {
            args[i++] = token;
        }
    }

    // add NULL at the end so that execvp and execute properly
    args[i] = NULL;

    // print statement from template_shell, but don't need it for now
    // for (int n = 0; n < i; n++)
    // {
    //     printf("getcm %s\n", args[n]);
    // }

    return i;
}

int main(void)
{
    char *args[LENGTH];
    int redirection;                              /* flag for output redirection */
    int piping;                                   /* flag for piping */
    int bg;                                       /* flag for running processes in the background */
    int cnt;                                      /* count of the arguments in the command */
    char *builtin[6];                             // list of built in commands
    int hasExecuted;                              // if a built in command is executed
    char *new_args[LENGTH];                       // to record the second command if pipe or redirection is ginven
    int *bgprocess = malloc(sizeof(int[LENGTH])); // to record all background process

    for (int i = 0; i < LENGTH; i++)
    {
        bgprocess[i] = 0;
    }

    // Check for signals
    if (signal(SIGINT, handle_sigint) == SIG_ERR)
    {
        printf("ERROR: could not bind signal handler for SIGINT\n");
        exit(1);
    }

    if (signal(SIGCHLD, handle_sigchild) == SIG_ERR)
    {
        printf("ERROR: could not bind signal handler for SIGINT\n");
        exit(1);
    }

    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR)
    {
        printf("ERROR: could not bind signal handler for SIGTSTP\n");
        exit(1);
    }

    int status;

    // a list of builtin command to execute in the parent process
    builtin[0] = "cd";
    builtin[1] = "echo";
    builtin[2] = "exit";
    builtin[3] = "pwd";
    builtin[4] = "jobs";
    builtin[5] = "fg";

    while (1)
    {
        // reset flags
        bg = 0;
        redirection = 0;
        piping = 0;
        hasExecuted = 0;

        if ((cnt = getcmd("\n>> ", args, &bg)) == 0)
        {
            printf("Invalid command\n");
            continue;
        }

        // do a loop to the background process table and clear finished child
        for (int i = 0; i < LENGTH; i++)
        {
            pid_t pid;
            pid = waitpid(bgprocess[i], &status, WNOHANG);
            if (pid != 0)
            {
                bgprocess[i] = 0;
            }
        }

        // l records the the first command to args
        int l = 0;
        // in case theres piping or redirection, pi_re_loc records the next command to new_args
        int pi_re_loc = 0;

        // record the command until the end or encounter '|' or '>'
        for (l; l < cnt; l++)
        {
            if (strcmp(args[l], "|") == 0)
            {
                piping = 1;
                args[l] = NULL;
                break;
            }
            else if (strcmp(args[l], ">") == 0)
            {
                redirection = 1;
                args[l] = NULL;
                break;
            }
        }

        // if the piping or redirection is 1, then it means there were unrecorded command from the last loop
        // start a new loop and copy the rest to a new array
        if (piping == 1 || redirection == 1)
        {
            for (l; l < cnt; l++)
            {
                new_args[pi_re_loc] = args[l + 1];
                pi_re_loc++;
            }
        }

        // section for built in commands
        for (int i = 0; i < 6; i++)
        {
            // check if the args is part of built in command and if so execute it
            if (strcmp(builtin[i], args[0]) == 0)
            {

                if (strcmp(args[0], "exit") == 0)
                {
                    free(bgprocess); // free the memory space of background table before exit
                    exit(0);
                }

                // get pwd using syscall and then print current location
                if (strcmp(args[0], "pwd") == 0)
                {
                    char cwd[256];
                    getwd(cwd);
                    printf("%s\n", cwd);
                }

                // use change dir syscall
                if (strcmp(args[0], "cd") == 0)
                {
                    chdir(args[1]);
                }

                // print all arguments
                if (strcmp(args[0], "echo") == 0)
                {
                    for (int n = 1; n < cnt; n++)
                    {
                        printf("%s ", args[n]);
                    }
                    printf("\n");
                }

                // print all non 0 processes in the background table and its indexes
                if (strcmp(args[0], "jobs") == 0)
                {

                    for (int i = 0; i < LENGTH; i++)
                    {
                        if (bgprocess[i] != 0)
                        {
                            printf("[%d]  process ID: %d\n", i, bgprocess[i]);
                        }
                    }
                }

                // find the process id in the specific index and make parent process wait
                if (strcmp(args[0], "fg") == 0)
                {
                    waitpid(bgprocess[atoi(args[1])], NULL, 0);
                }

                hasExecuted = 1;
                break;
            }
        }

        // if a built in command is executed then receive new input again
        if (hasExecuted != 0)
        {
            continue;
        }

        // Not built-in commands then create child process
        int pid = fork();

        // For the child process
        if (pid == 0)
        {

            if (piping == 1 || redirection == 1)
            {
                // starting piping porcess
                int pipefd[2];
                pipe(pipefd);

                // if redirect to a file then create a file from the second argument and make the result of first arg as input
                if (redirection == 1)
                {
                    close(pipefd[0]);
                    int fd = open(new_args[0], O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
                    //  dup2(fd, 1);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    execvp(args[0], args);
                    close(pipefd[1]);
                    exit(0);
                }

                // if it'ss piping then fork again and let the
                // child command give input and parent wait for input and execute the command
                else
                {

                    int new_pid = fork();

                    // if in child porcess
                    if (new_pid == 0)
                    {
                        close(pipefd[0]);
                        dup2(pipefd[1], STDOUT_FILENO);
                        close(pipefd[1]);
                        if (execvp(args[0], args) < 0)
                        {
                            printf("exec failed\n");
                            exit(1);
                        }
                        exit(0);
                    }
                    else if (new_pid == -1)
                    {
                        printf("ERROR: fork failed\n");
                        exit(1);
                    }
                    // parent process of the piping procedure
                    else
                    {
                        // wait the child to finish and then read the input and execute subsequent command
                        waitpid(new_pid, NULL, 0);

                        close(pipefd[1]);
                        dup2(pipefd[0], STDIN_FILENO);
                        close(pipefd[0]);

                        if (execvp(new_args[0], new_args) < 0)
                        {
                            printf("exec failed\n");
                            exit(1);
                        }
                        exit(0);
                    }
                }
            }

            // if no piping or redirection then execute command normally
            else
            {
                if (execvp(args[0], args) < 0)
                {
                    printf("exec failed\n");
                    exit(1);
                }

                exit(0); /* child termination */
            }
        }

        else if (pid == -1)
        {
            printf("ERROR: fork failed\n");
            exit(1);
        }

        // For the parent process
        else
        {
            // If the child process is not in the bg => wait for it
            if (bg == 0)
            {
                waitpid(pid, NULL, 0);
            }
            else
            {
                // find an empty slot in background table and store child id
                for (int i = 0; i < LENGTH; i++)
                {
                    if (bgprocess[i] == 0)
                    {
                        bgprocess[i] = pid;
                        break;
                    }
                }

                // go receive user's next command
                continue;
            }
        }
    }
}