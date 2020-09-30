#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_ARGS 10
#define MAX_ARGS_LEN 128
#define MAX_PATH_LIST 256
#define MAX_TOKEN_SIZE 256
#define MAX_HIST_SIZE 10
#define MAX_LINE_SIZE 256

struct History {
    int size;
    int head;
    char** hist_elements;
};

static int num_args_type_np = 0;
static int num_args_type_dp = 0;
static int num_args_type_tp = 0;
static int num_args_type_sp = 0;

static char* command_type = NULL;
static char* PATH = NULL;
struct History* history;

void main_loop();
char* read_command();
char** parse_command(char*);
int shell_exec(char**, char*);
void debug(char**, int);
void string_tokenizer(char*, char**, char*, int*);
char* trimwhitespace(char*);
void signal_handler(int);
void init_hist(int);
void add_hist(char*);
void print_hist();

void init_hist(int size) {
    history = (struct History*) malloc(1*sizeof(struct History));
    history->size = size;
    history->hist_elements =(char**) calloc(size, sizeof(char*));
}

void add_hist(char* input) {
  if (history->hist_elements[history->head] != NULL) {
    free(history->hist_elements[history->head]);
  }
  history->hist_elements[history->head] = (char*) calloc(MAX_LINE_SIZE, sizeof(char));
  strcpy(history->hist_elements[history->head], input);
  history->head = (history->head + 1) % history->size;
}

void print_hist() {
  int start = history->head;
  for (int i = 0; i < history->size; i++) {
    if (history->hist_elements[start] != NULL) {
      printf("%s\n", history->hist_elements[start]);
    }
    start++;
    if (start >= history->size) {
      start = 0;
    }
  }
}

void signal_handler(int sig) {
    if(sig == SIGINT) {
        printf("Printing last 10 commands: \n");
        print_hist();
    }

    if(sig == SIGQUIT) {
        char ch;
        printf("\nDo you really want to exit ? Y or N. \n");
        scanf("%c", &ch);
        if(ch == 'Y' || ch == 'y') {
            exit(EXIT_SUCCESS);
        }
        else {
            printf("\n");
            return;
        }
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGCHLD, SIG_DFL);

    for (int sig = 1; sig < NSIG; sig++) {
        if (sig != SIGINT && sig != SIGQUIT && sig != SIGCHLD) {
            signal(sig, SIG_IGN);
        }
    }
    init_hist(MAX_HIST_SIZE);
    main_loop();
    free(history);
    return EXIT_SUCCESS;
}

void main_loop() {
    char* command;
    char** parsed_commands;
    int status = 1;
    PATH = getenv("PATH");
    while(status) {
        printf("\npshell > ");
        fflush(stdout);
        command = read_command();
        if(strlen(command) == 0) {
            continue;
        }
        add_hist(command);
        parsed_commands = parse_command(command);
        status = shell_exec(parsed_commands, command_type);
    }

    free(command);
    free(parsed_commands);
}

char* read_command() {
    char* input = NULL;
    ssize_t bufsize = 0;

    if(getline(&input, &bufsize, stdin) == -1) {
        if(feof(stdin)) {
            exit(EXIT_SUCCESS);
        }
        else {
            printf("pshell: ERR %d: read_command\n", errno);
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
    }
    input = trimwhitespace(input);
    return input;
}

char* trimwhitespace(char *str) {
    if(str == NULL) {
        return str;
    }

    char* end;
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)
        return str;

    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}


char** parse_command(char* command) {
    char** parsed_command;
    parsed_command = (char**) calloc(MAX_ARGS+1, sizeof(char *));

    if(strcmp(command, "exit") == 0) {
        exit(EXIT_SUCCESS);
    }
    if(strcmp(command, "EXIT") == 0) {
        exit(EXIT_SUCCESS);
    }

    if(strstr(command, "|||") != NULL) {
        command_type = "TP";
        string_tokenizer(command, parsed_command, "|||", &num_args_type_tp);
    }
    else if(strstr(command, "||") != NULL) {
        command_type = "DP";
        string_tokenizer(command, parsed_command, "||", &num_args_type_dp);
    }
    else if(strstr(command, "|") != NULL) {
        command_type = "SP";
        string_tokenizer(command, parsed_command, "|", &num_args_type_sp);
    }
    else {
        command_type = "NP";
        string_tokenizer(command, parsed_command, " ", &num_args_type_np);
    }
    return parsed_command;
}

void string_tokenizer(char* command, char** parsed_command, char* delim, int* num_args) {
    int i = 0;
    char* token = strtok(command, delim);
    token = trimwhitespace(token);
    while(token != NULL) {
        parsed_command[i] = (char*) calloc(strlen(token), sizeof(char));
        parsed_command[i] = token;
        i++;
        token = strtok(NULL, delim);
        token = trimwhitespace(token);
    }
    *num_args = i;
}

int shell_exec(char** parsed_command, char* type) {

    if(strcmp(type, "NP") == 0) {
        char** path_list = (char**) calloc(MAX_PATH_LIST+1, sizeof(char *));

        char* path = (char *) calloc(strlen(PATH), sizeof(char));
        strcpy(path, PATH);
        int i = 0;
        char* token = strtok(path, ":");
        while(token != NULL) {
            path_list[i] = (char*) calloc(MAX_TOKEN_SIZE, sizeof(char));
            path_list[i] = token;
            i++;
            token = strtok(NULL, ":");
        }

        pid_t childpid;
        int status;
        i = 0;
        
        while(path_list[i] != NULL) {
            printf("************************************************************\n");
            char* main_comm = parsed_command[0];
            char* buff = (char*) calloc(MAX_TOKEN_SIZE, sizeof(char));
            strcpy(buff, path_list[i]);
            strcat(buff, "/");
            strcat(buff, parsed_command[0]);
    
            parsed_command[0] = buff;
            if(access(buff, R_OK) == 0) {

                if((childpid = fork()) == -1) {
                printf("pshell : ERR %d : Command Type NP Cannot fork\n", errno);
                exit(1);
                }
                else if(childpid == 0) {
                    printf("Executing %s\n", buff);
                    
                    char* args_vec[num_args_type_np+1];
                    if(num_args_type_np == 1) {
                        args_vec[0] = parsed_command[0];
                        args_vec[1] = NULL;
                    }
                    else {
                        for(int j = 0; j < num_args_type_np+1; j++) {
                            args_vec[j] = parsed_command[j];
                        }
                        args_vec[num_args_type_np] = NULL;
                    }
                    
                    if(execv(buff, args_vec) < 0) {
                        printf("pshell : ERR %d : Cannot exec command\n", errno);
                    }
                }
                else {
                    while(wait(&status) != childpid);
                    printf("Command Successful : PID %d\n", childpid);
                    printf("Status: %d\n", status);
                }
            }
            else {
                if (errno == ENOENT) 
                    printf ("%s does not exist\n", buff);
                else if (errno == EACCES) 
                    printf ("%s is not accessible\n", buff);
            }
            parsed_command[0] = main_comm;
            free(buff);
            i++;
        }
        free(path_list);
        free(path);
    }
    else if(strcmp(type, "SP") == 0) {
        int num_pipes = num_args_type_sp - 1;

        char* command_vec[num_args_type_sp+1];
        if(num_args_type_sp == 1) {
            command_vec[0] = parsed_command[0];
            command_vec[1] = NULL;
        }
        else {
            for(int j = 0; j < num_args_type_sp+1; j++) {
                command_vec[j] = parsed_command[j];
            }
            command_vec[num_args_type_sp] = NULL;
        }
        
        int pfd[2];
        int fdd = 0;

        int i = 0;
        while(command_vec[i] != NULL) {
            fprintf(stderr, "Starting %s\n", command_vec[i]);
            if(pipe(pfd) == -1) {
                fprintf(stderr,"pshell : ERR %d: Error in Pipe\n", errno);
            }
            fprintf(stderr, "Pipe Created\n");
            char** command_args = NULL;
            
            switch(fork()) {
                case -1:
                    fprintf(stderr,"pshell : ERR %d : Error in fork NP\n", errno);
                case 0: {
                    fprintf(stderr, "PID of %s : %d\n", command_vec[i], getpid());
                    if(i != 0) {
                        if(fdd != STDIN_FILENO) {
                            if(dup2(fdd, STDIN_FILENO) == -1) {
                                fprintf(stderr,"pshell : ERR %d: Error in pfd dup read end in %s prgm\n", errno, command_vec[i]);
                                exit(EXIT_FAILURE);
                            }
                            fprintf(stderr, "Connected STDIN to pipe %s\n", command_vec[i]);
                            if(close(fdd) == -1) {
                                fprintf(stderr,"pshell : ERR %d: Error in pfd closing 1 read end in %s prgm\n", errno, command_vec[i]);
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                    
                    if(command_vec[i+1] != NULL) {
                        if(pfd[1] != STDOUT_FILENO) {
                            if(dup2(pfd[1], STDOUT_FILENO) == -1) {
                                fprintf(stderr,"pshell : ERR %d: Error in pfd dup write end in %s prgm\n", errno, command_vec[i]);
                                exit(EXIT_FAILURE);
                            }
                            fprintf(stderr, "Connected STDOUT to pipe %s\n", command_vec[i]);
                            if(close(pfd[1]) == -1) {
                                fprintf(stderr,"pshell : ERR %d: Error in pfd closing 1 write end in %s prgm\n", errno, command_vec[i]);
                                exit(EXIT_FAILURE);
                            }
                        }
                    }
                    
                    if(i == 0) {
                        if(close(pfd[0]) == -1) {
                            fprintf(stderr,"pshell : ERR %d: Error in pfd closing 2 read end in %s prgm\n", errno, command_vec[i]);
                            exit(EXIT_FAILURE);
                        }
                    }
                    
                    if(command_vec[i+1] == NULL) {
                        if(close(pfd[1]) == -1) {
                            fprintf(stderr,"pshell : ERR %d: Error in pfd closing 2 write end in %s prgm\n", errno, command_vec[i]);
                            exit(EXIT_FAILURE);
                        }
                    }

                    char* command = command_vec[i];
                    int num_command_args;
                    command_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                    string_tokenizer(command_vec[i], command_args, " ", &num_command_args);
                
                    char* args_vec[num_command_args+1];
                    if(num_command_args == 1) {
                        args_vec[0] = command_args[0];
                        args_vec[1] = NULL;
                    }
                    else {
                        for(int j = 0; j < num_command_args+1; j++) {
                            args_vec[j] = command_args[j];
                        }
                        args_vec[num_command_args] = NULL;
                    }
                    fprintf(stderr,"Executing %s\n", command);
                    execvp(command_args[0], args_vec);
                    fprintf(stderr,"pshell : ERR %d : error in exec in %s program\n", errno, command);
                    exit(EXIT_FAILURE);

                }
                default:
                    break;
            }
            if(close(pfd[1]) == -1) {
                printf("pshell : ERR %d: Error in pfd closing write end in %s prgm\n", errno, command_vec[i]);
                exit(EXIT_FAILURE);
            }
            fdd = pfd[0];
            free(command_args);
            i++;
        }
        fprintf(stderr, "Waiting for all children to finish\n");
        while(num_args_type_sp-- > 0) {
            if(wait(NULL) == -1) {
                printf("pshell : ERR %d : waiting for child Main\n", errno);
            }
        }
        fprintf(stderr, "Command Run Successful\n");
    }
    else if(strcmp(type, "DP") == 0) {
    
        char* left_pipe = parsed_command[0];
        char* right_pipe = parsed_command[1];
        char** parsed_right_pipe = NULL;
        int num;
        parsed_right_pipe = (char**) calloc(MAX_ARGS+1, sizeof(char *));
        string_tokenizer(right_pipe, parsed_right_pipe, ",", &num);
        char* first_program = parsed_right_pipe[0];
        char* second_program = parsed_right_pipe[1];

        if (mkfifo ("/tmp/fifo1", S_IRUSR| S_IWUSR) < 0) {
            printf("pshell : ERR %d: Error creating FIFO\n", errno);
            return 1;
        }

        char** first_prog_args = NULL;
        switch(fork()) {
            case -1:
                printf("pshell : ERR %d : Error in fork first prgm\n", errno);
            case 0: {
                int fd;
                fd = open("/tmp/fifo1", O_RDONLY);

                if(fd != STDIN_FILENO) {
                    if(dup2(fd, STDIN_FILENO) == -1) {
                        printf("pshell : ERR %d : Error in dup read end in first prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(fd) == -1) {
                        printf("pshell : ERR %d : Error in closing read end in first prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }

                int num_first_args_right;
                first_prog_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                string_tokenizer(first_program, first_prog_args, " ", &num_first_args_right);
                
                char* args_vec[num_first_args_right+1];
                if(num_first_args_right == 1) {
                    args_vec[0] = first_prog_args[0];
                    args_vec[1] = NULL;
                }
                else {
                    for(int j = 0; j < num_first_args_right+1; j++) {
                        args_vec[j] = first_prog_args[j];
                    }
                    args_vec[num_first_args_right] = NULL;
                }
                // printf("Executing %s\n", first_program);
                execvp(first_prog_args[0], args_vec);
                printf("pshell : ERR %d : error in exec in first program\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }
        free(first_prog_args);

        int f_pfd[2];
        if(pipe(f_pfd) == -1) {
            printf("pshell : ERR %d: Error in Pipe\n", errno);
        }
        
        char** left_pipe_args = NULL;
        switch(fork()) {
            case -1:
                printf("pshell : ERR %d : Error in Fork for DP Command type\n", errno);
            case 0: {
                if(close(f_pfd[0]) == -1) {
                    printf("pshell : ERR %d : Error in closing read fd for left prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                
                if(f_pfd[1] != STDOUT_FILENO) {
                    if(dup2(f_pfd[1], STDOUT_FILENO) == -1) {
                        printf("pshell : ERR %d : Cannot dup write end in left prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(f_pfd[1]) == -1) {
                        printf("pshell : ERR %d : cannot close write end in left prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }
                
                int num_args_left;
                left_pipe_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                string_tokenizer(left_pipe, left_pipe_args, " ", &num_args_left);
                
                char* args_vec[num_args_left+1];
                if(num_args_left == 1) {
                    args_vec[0] = left_pipe_args[0];
                    args_vec[1] = NULL;
                }
                else {
                    for(int j = 0; j < num_args_left+1; j++) {
                        args_vec[j] = left_pipe_args[j];
                    }
                    args_vec[num_args_left] = NULL;
                }

                // printf("Executing %s\n", left_pipe);
                execvp(left_pipe_args[0], args_vec);
                printf("pshell : ERR %d : cannot exec left prgm\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }
        free(left_pipe_args);

        int s_pfd[2];
        if(pipe(s_pfd) == -1) {
            printf("pshell : ERR %d: Error in Pipe\n", errno);
        }

        switch(fork()) {
            case -1:
                printf("pshell : ERR %d: Error in fork tee prgm\n", errno);
            case 0: {
                if(close(f_pfd[1]) == -1) {
                    printf("pshell : ERR %d: cannot close f_pfd write end in tee prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                if(close(s_pfd[0]) == -1) {
                    printf("pshell : ERR %d: cannot close s_pfd write end in tee prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                
                if(f_pfd[0] != STDIN_FILENO) {
                    if(dup2(f_pfd[0], STDIN_FILENO) == -1) {
                        printf("pshell : ERR %d: Error in f_pfd dup read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(f_pfd[0]) == -1) {
                        printf("pshell : ERR %d: Error in f_pfd closing read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }
                
                if(s_pfd[1] != STDOUT_FILENO) {
                    if(dup2(s_pfd[1], STDOUT_FILENO) == -1) {
                        printf("pshell : ERR %d: Error in s_pfd dup read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(s_pfd[1]) == -1) {
                        printf("pshell : ERR %d: Error in s_pfd closing read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }

                printf("Executing Tee\n");
                execlp("tee", "tee", "/tmp/fifo1", NULL);
                printf("pshell : ERR %d : error in exec in tee program\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }

        if(close(f_pfd[0]) == -1) {
            printf("pshell : ERR %d : error closing read end of first pipe Main\n", errno);
        }
        if(close(f_pfd[1]) == -1) {
            printf("pshell : ERR %d : error closing write end of first pipe Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        
        char** second_prog_args = NULL;
        switch(fork()) {
            case -1:
                printf("pshell : ERR %d : Error in fork second prgm\n", errno);
            case 0: {
                if(close(s_pfd[1]) == -1) {
                    printf("pshell : ERR %d: cannot close s_pfd write end in tee prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                
                if(s_pfd[0] != STDIN_FILENO) {
                    if(dup2(s_pfd[0], STDIN_FILENO) == -1) {
                        printf("pshell : ERR %d : Error in dup read end in second prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(s_pfd[0]) == -1) {
                        printf("pshell : ERR %d : Error in closing read end in second prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }
                
                int num_second_args_right;
                second_prog_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                string_tokenizer(second_program, second_prog_args, " ", &num_second_args_right);
                
                char* args_vec[num_second_args_right+1];
                if(num_second_args_right == 1) {
                    args_vec[0] = second_prog_args[0];
                    args_vec[1] = NULL;
                }
                else {
                    for(int j = 0; j < num_second_args_right+1; j++) {
                        args_vec[j] = second_prog_args[j];
                    }
                    args_vec[num_second_args_right] = NULL;
                }
                // printf("Executing %s\n", second_program);
                execvp(second_prog_args[0], args_vec);
                printf("pshell : ERR %d : error in exec in second program\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }
        free(second_prog_args);

        if(close(s_pfd[0]) == -1) {
            printf("pshell : ERR %d : error closing read end of second pipe Main\n", errno);
        }
        if(close(s_pfd[1]) == -1) {
            printf("pshell : ERR %d : error closing write end of second pipe Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }

        switch(fork()) {
            case -1:
                printf("pshell : ERR %d : Error in Fork for DP Command type\n", errno);
            case 0:
                execlp("rm", "rm", "-r", "/tmp/fifo1", NULL);
                printf("pshell : ERR %d : Cannot delete FIFO\n", errno);
                exit(EXIT_FAILURE);
            default:
                break;
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        free(parsed_right_pipe);
    }
    else if(strcmp(type, "TP") == 0) {
        char* left_pipe = parsed_command[0];
        char* right_pipe = parsed_command[1];
        char** parsed_right_pipe = NULL;
        int num;
        parsed_right_pipe = (char**) calloc(MAX_ARGS+1, sizeof(char *));
        string_tokenizer(right_pipe, parsed_right_pipe, ",", &num);
        char* first_program = parsed_right_pipe[0];
        char* second_program = parsed_right_pipe[1];
        char* third_program = parsed_right_pipe[2];

        if (mkfifo ("/tmp/fifo1", S_IRUSR| S_IWUSR) < 0) {
            printf("pshell : ERR %d: Error creating FIFO1\n", errno);
            return 1;
        }
        if (mkfifo ("/tmp/fifo2", S_IRUSR| S_IWUSR) < 0) {
            printf("pshell : ERR %d: Error creating FIFO2\n", errno);
            return 1;
        }

        char** first_prog_args = NULL;
        switch(fork()) {
            case -1:
                printf("pshell : ERR %d: Error in fork first prgm\n", errno);
            case 0: {
                int fd;
                fd = open("/tmp/fifo1", O_RDONLY);

                if(fd != STDIN_FILENO) {
                    if(dup2(fd, STDIN_FILENO) == -1) {
                        printf("pshell : ERR %d : Error in dup read end in first prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(fd) == -1) {
                        printf("pshell : ERR %d : Error in closing read end in first prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }

                int num_first_args_right;
                first_prog_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                string_tokenizer(first_program, first_prog_args, " ", &num_first_args_right);
                
                char* args_vec[num_first_args_right+1];
                if(num_first_args_right == 1) {
                    args_vec[0] = first_prog_args[0];
                    args_vec[1] = NULL;
                }
                else {
                    for(int j = 0; j < num_first_args_right+1; j++) {
                        args_vec[j] = first_prog_args[j];
                    }
                    args_vec[num_first_args_right] = NULL;
                }
                // printf("Executing %s\n", first_program);
                execvp(first_prog_args[0], args_vec);
                printf("pshell : ERR %d : error in exec in first program\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }
        free(first_prog_args);

        char** second_prog_args = NULL;
        switch(fork()) {
            case -1:
                printf("pshell : ERR %d: Error in fork second prgm\n", errno);
            case 0: {
                int fd;
                fd = open("/tmp/fifo2", O_RDONLY);

                if(fd != STDIN_FILENO) {
                    if(dup2(fd, STDIN_FILENO) == -1) {
                        printf("pshell : ERR %d : Error in dup read end in second prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(fd) == -1) {
                        printf("pshell : ERR %d : Error in closing read end in second prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }

                int num_second_args_right;
                second_prog_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                string_tokenizer(second_program, second_prog_args, " ", &num_second_args_right);
                
                char* args_vec[num_second_args_right+1];
                if(num_second_args_right == 1) {
                    args_vec[0] = second_prog_args[0];
                    args_vec[1] = NULL;
                }
                else {
                    for(int j = 0; j < num_second_args_right+1; j++) {
                        args_vec[j] = second_prog_args[j];
                    }
                    args_vec[num_second_args_right] = NULL;
                }
                // printf("Executing %s\n", second_program);
                execvp(second_prog_args[0], args_vec);
                printf("pshell : ERR %d : error in exec in second program\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }
        free(second_prog_args);

        int f_pfd[2];
        if(pipe(f_pfd) == -1) {
            printf("pshell : ERR %d: Error in Pipe\n", errno);
        }
        
        char** left_pipe_args = NULL;
        switch(fork()) {
            case -1:
                printf("pshell : ERR %d: Error in Fork for DP Command type\n", errno);
            case 0: {
                if(close(f_pfd[0]) == -1) {
                    printf("pshell : ERR %d : Error in closing read fd for left prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                
                if(f_pfd[1] != STDOUT_FILENO) {
                    if(dup2(f_pfd[1], STDOUT_FILENO) == -1) {
                        printf("pshell : ERR %d : Cannot dup write end in left prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(f_pfd[1]) == -1) {
                        printf("pshell : ERR %d : cannot close write end in left prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }
                
                int num_args_left;
                left_pipe_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                string_tokenizer(left_pipe, left_pipe_args, " ", &num_args_left);
                
                char* args_vec[num_args_left+1];
                if(num_args_left == 1) {
                    args_vec[0] = left_pipe_args[0];
                    args_vec[1] = NULL;
                }
                else {
                    for(int j = 0; j < num_args_left+1; j++) {
                        args_vec[j] = left_pipe_args[j];
                    }
                    args_vec[num_args_left] = NULL;
                }

                // printf("Executing %s\n", left_pipe);
                execvp(left_pipe_args[0], args_vec);
                printf("pshell : ERR %d : cannot exec left prgm\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }
        free(left_pipe_args);

        int s_pfd[2];
        if(pipe(s_pfd) == -1) {
            printf("pshell : ERR %d: Error in Pipe\n", errno);
        }

        switch(fork()) {
            case -1:
                printf("pshell : ERR %d: Error in fork tee prgm\n", errno);
            case 0: {
                if(close(f_pfd[1]) == -1) {
                    printf("pshell : ERR %d: cannot close f_pfd write end in tee prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                if(close(s_pfd[0]) == -1) {
                    printf("pshell : ERR %d: cannot close s_pfd write end in tee prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                
                if(f_pfd[0] != STDIN_FILENO) {
                    if(dup2(f_pfd[0], STDIN_FILENO) == -1) {
                        printf("pshell : ERR %d: Error in f_pfd dup read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(f_pfd[0]) == -1) {
                        printf("pshell : ERR %d: Error in f_pfd closing read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }
                
                if(s_pfd[1] != STDOUT_FILENO) {
                    if(dup2(s_pfd[1], STDOUT_FILENO) == -1) {
                        printf("pshell : ERR %d: Error in s_pfd dup read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(s_pfd[1]) == -1) {
                        printf("pshell : ERR %d: Error in s_pfd closing read end in tee prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }

                printf("Executing Tee\n");
                execlp("tee", "tee", "/tmp/fifo1", "/tmp/fifo2", NULL);
                printf("pshell : ERR %d : error in exec in tee program\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }

        if(close(f_pfd[0]) == -1) {
            printf("pshell : ERR %d : error closing read end of first pipe Main\n", errno);
        }
        if(close(f_pfd[1]) == -1) {
            printf("pshell : ERR %d : error closing write end of first pipe Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d : waiting for child Main\n", errno);
        }
        
        char** third_prog_args = NULL;
        switch(fork()) {
            case -1:
                printf("pshell : ERR %d : Error in fork third prgm\n", errno);
            case 0: {
                if(close(s_pfd[1]) == -1) {
                    printf("pshell : ERR %d: cannot close s_pfd write end in third prgm\n", errno);
                    exit(EXIT_FAILURE);
                }
                
                if(s_pfd[0] != STDIN_FILENO) {
                    if(dup2(s_pfd[0], STDIN_FILENO) == -1) {
                        printf("pshell : ERR %d : Error in dup read end in third prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                    if(close(s_pfd[0]) == -1) {
                        printf("pshell : ERR %d: Error in closing read end in third prgm\n", errno);
                        exit(EXIT_FAILURE);
                    }
                }
                int num_third_args_right;
                third_prog_args = (char**) calloc(MAX_ARGS+1, sizeof(char *));
                string_tokenizer(third_program, third_prog_args, " ", &num_third_args_right);
                
                char* args_vec[num_third_args_right+1];
                if(num_third_args_right == 1) {
                    args_vec[0] = third_prog_args[0];
                    args_vec[1] = NULL;
                }
                else {
                    for(int j = 0; j < num_third_args_right+1; j++) {
                        args_vec[j] = third_prog_args[j];
                    }
                    args_vec[num_third_args_right] = NULL;
                }
                // printf("Executing %s\n", third_program);
                execvp(third_prog_args[0], args_vec);
                printf("pshell : ERR %d: error in exec in third program\n", errno);
                exit(EXIT_FAILURE);
            }
            default:
                break;
        }
        free(third_prog_args);
        
        if(close(s_pfd[0]) == -1) {
            printf("pshell : ERR %d: error closing read end of second pipe Main\n", errno);
        }
        if(close(s_pfd[1]) == -1) {
            printf("pshell : ERR %d: error closing write end of second pipe Main\n", errno);
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d: waiting for child Main\n", errno);
        }

        switch(fork()) {
            case -1:
                printf("pshell : ERR %d: Error in Fork for DP Command type\n", errno);
            case 0:
                execlp("rm", "rm", "-r", "/tmp/fifo1", NULL);
                printf("pshell : ERR %d: Cannot delete FIFO1\n", errno);
                exit(EXIT_FAILURE);
            default:
                break;
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d: waiting for child Main\n", errno);
        }

        switch(fork()) {
            case -1:
                printf("pshell : ERR %d: Error in Fork for DP Command type\n", errno);
            case 0:
                execlp("rm", "rm", "-r", "/tmp/fifo2", NULL);
                printf("pshell : ERR %d: Cannot delete FIFO2\n", errno);
                exit(EXIT_FAILURE);
            default:
                break;
        }
        if(wait(NULL) == -1) {
            printf("pshell : ERR %d: waiting for child Main\n", errno);
        }
        free(parsed_right_pipe);
    }
    else {
        printf("pshell : ERR %d: Command not recognized\n", errno);
        fflush(stdout);
    }
    return 1;
}

void debug(char** parse_command, int num) {
   for(int i = 0; i < num; i++) {
       printf("%s\n", *(&parse_command[i]));
   }
}