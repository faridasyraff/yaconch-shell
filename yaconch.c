#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h> //global variable set by kernel when syscalls are interrupted
#include <fcntl.h> // get expln
#include <stdint.h>
/*GLOBAL VARIABLES*/

/*STRUCTS*/
typedef struct{
        char** args;
        char* input_file;
        char* output_file;
        int append_flag;
} Command;

typedef struct{
	Command* command;
	int commandCnts;
	int background_flag;
} Commands;

#define MAX_BACKGROUND_JOBS 64
typedef struct{
	int jobCnts;
	int finishedJobsFlag;
	pid_t finishedJobs[MAX_BACKGROUND_JOBS];
	int index;
} BgJobTracker;
BgJobTracker bjt = {0}; // global region

/*FORWARD DECLARATION*/
void yaconch_loop(void);
char* yaconch_read_line(void);
Commands yaconch_parser_cmds(char* line);
Command yaconch_parser_cmd(char* line, Commands* commands);
int yaconch_exec(Commands command);
int yaconch_launch_program(Commands commands);
int yaconch_cd(char** args);
int yaconch_help(char** args);
int yaconch_exit(char** args);
int yaconch_builtint_cnt(void);

void yaconch_handle_sigint(void);
void yaconch_handler_sigint(int);
void yaconch_handle_sigtstp(void);
void yaconch_handler_sigtstp(int);
void yaconch_handle_sigchld(void);
void yaconch_handler_sigchld(int);

/*SIGNAL HANDLING*/
void yaconch_handle_sigint(){
	struct sigaction sa;
	sa.sa_handler = yaconch_handler_sigint;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGINT);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
}

void yaconch_handle_sigtstp(){
	struct sigaction sa;
	sa.sa_handler = yaconch_handler_sigtstp;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGTSTP);
	sa.sa_flags = 0; // handle syscall interrupted, SA_RESTART for continuing

	sigaction(SIGTSTP, &sa, NULL);
}

void yaconch_handle_sigchld(){
	struct sigaction sa;
	sa.sa_handler = yaconch_handler_sigchld;
	sigemptyset(&sa.sa_mask); // empty mask first
	sigaddset(&sa.sa_mask, SIGCHLD); // mask tells which signal to handle
	sa.sa_flags = SA_RESTART;

	sigaction(SIGCHLD, &sa, NULL);
}

void yaconch_handler_sigtstp(int sig){
	write(STDOUT_FILENO, "\n", 1);
}

void yaconch_handler_sigint(int sig){
	write(STDOUT_FILENO, "\n", 1);
}

void yaconch_handler_sigchld(int sig){
	// tells parent to wait
	pid_t pid;
	int status;
	do{
		pid = waitpid(-1, &status, WNOHANG);
		if(pid > 0){
			bjt.finishedJobs[bjt.index] = pid;
			bjt.index++;
			bjt.finishedJobsFlag = 1;
		}
	}while(pid > 0); // waitpid returns 0 on WNOHANG and still sleeping child, -1 on failure, stop looping then
}

/*MAIN LOOP*/
void yaconch_loop(void){
	char* line;
	Commands command;
	int status = 1;

	do{
		if(bjt.finishedJobsFlag == 1){
			bjt.finishedJobsFlag = 0;
			for(int i = 0; i < bjt.jobCnts; i++){
				printf("pid[%d] done!\n", bjt.finishedJobs[i]);
			}
			bjt.index = 0;
		}
		printf(">>");
		line = yaconch_read_line();
		if(strlen(line) == 0){
			free(line);
			fflush(stdout); // fflush clears the printf buffer and force anything in buffer to appear in console
			continue;
		}
		command = yaconch_parser_cmds(line);
		int n = command.commandCnts;
		status = yaconch_exec(command);

		free(line);
		for(int i = 0; i < n; i++){
			free(command.command[i].args);
		}
	}while(status);
 }

int main(int argc, char** argv){
	yaconch_handle_sigint();
	yaconch_handle_sigtstp();
	yaconch_handle_sigchld();
	if(argc > 1 && strcmp(argv[1], "--version") == 0){ // version information
		printf("Yaconch Shell by Muhammad Farid Asyraf\n0.2.0\n");
		return EXIT_SUCCESS;
	}
	yaconch_loop();
	return EXIT_SUCCESS;
}

/*LINE READING AND PARSING*/
#define STD_BUFF_SIZE 1024
char* yaconch_read_line(void){
	int bufsize = STD_BUFF_SIZE;
	int idx = 0;
	char* buffer = malloc(sizeof(char)*bufsize);
	int c;

	if(!buffer){
		fprintf(stderr, "yaconch: allocation error!\n");
		exit(EXIT_FAILURE);
	}

	while(1){
		c = getchar();
		if(c == EOF || c == '\n'){
			if(c == EOF){// EOF from interrupt
				if(errno == EINTR){
					clearerr(stdin);
					printf(">>");
					continue;
				}else{//actual EOF
					buffer[idx] = '\0';
					return buffer;
				}
			}
		buffer[idx] = '\0';
		return buffer;
		}
		else{
			buffer[idx] = c;
		}
		idx++;
		//allocate extra space if buffer is full
		if(idx >= STD_BUFF_SIZE){
			bufsize = bufsize*2;
			buffer = realloc(buffer, sizeof(char)*bufsize);
			if(!buffer){
				fprintf(stderr, "yaconch: allocation error!\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}
// splits line by |
#define STD_CMDS_DELIMS "|"
#define YACONCH_PARSER_BUFSIZE  64
Commands yaconch_parser_cmds(char* line){
	char* saveptr;
	int bufsize = YACONCH_PARSER_BUFSIZE;
	Commands commands;
	commands.commandCnts = 0;
	int idx = 0;
	char* command;
	commands.background_flag = 0;
	commands.command = malloc(sizeof(Command) * bufsize);
	if(!commands.command){
		fprintf(stderr, "yaconch: allocation error!\n");
		exit(EXIT_FAILURE);
	}

	command = strtok_r(line, STD_CMDS_DELIMS, &saveptr);
	while(command != NULL){
		commands.command[idx] = yaconch_parser_cmd(command, &commands);
		idx += 1;
		command = strtok_r(NULL, STD_CMDS_DELIMS, &saveptr);
	}
	commands.commandCnts = idx;
	return commands;
}
#define STD_DELIMS " \t\n\a\r"
Command yaconch_parser_cmd(char* line, Commands* commands){
	int bufsize = YACONCH_PARSER_BUFSIZE;
	int idx = 0;
	Command command;
	command.args = malloc(sizeof(char*)*bufsize);
	command.input_file = NULL;
	command.output_file = NULL;
	command.append_flag = 0;
	char* arg;

	if(!command.args){
		fprintf(stderr, "yaconch: allocation error!\n");
		exit(EXIT_FAILURE);
	}
	arg = strtok(line, STD_DELIMS);
	while(arg != NULL){
		if(strcmp(arg, ">>") == 0){
			command.output_file = strtok(NULL, STD_DELIMS);
			command.append_flag = 1;
			arg = strtok(NULL, STD_DELIMS);
		}else if(strcmp(arg, ">") == 0){
			command.output_file = strtok(NULL, STD_DELIMS);
			arg = strtok(NULL, STD_DELIMS);
		}else if(strcmp(arg, "<") == 0){
			command.input_file = strtok(NULL, STD_DELIMS);
			arg = strtok(NULL, STD_DELIMS);
		}else if(strcmp(arg, "&") == 0){
			commands->background_flag = 1;
			arg = strtok(NULL, STD_DELIMS);
		}else{
			command.args[idx] = arg;
			idx++;

			if(idx >= bufsize){
				bufsize *= 2;
				command.args = realloc(command.args, sizeof(char*)*bufsize);
				if(!command.args){
					fprintf(stderr, "yaconch: allocation error!\n");
					exit(EXIT_FAILURE);
				}
			}
			arg = strtok(NULL, STD_DELIMS);
		}
	}
	command.args[idx] = NULL;
	return command;
}
/*LAUNCHER*/
int yaconch_launch_program(Commands commands){
	int status;
	int n = commands.commandCnts;
	int pipefd[n-1][2];
	pid_t pid[n];
	// pipe creation
	for(int i = 0; i < n - 1; i++){
		if(pipe(pipefd[i]) == -1){
			fprintf(stderr, "yaconch: pipe error!");
		}
	}
	// fork
	for(int i = 0; i < n; i++){
		char** args = commands.command[i].args;
		pid[i] = fork();
		// child
		if(pid[i] == 0){
			signal(SIGINT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);
			// pipe wiring
			if(i > 0){
				dup2(pipefd[i-1][0], 0);
			}
			if(i < n - 1){
				dup2(pipefd[i][1], 1);
			}
			// redirection handling -> overrides piping
			if(commands.command[i].output_file != NULL){
				int open_flags = O_WRONLY | O_CREAT;
				open_flags |= commands.command[i].append_flag ? O_APPEND : O_TRUNC;
				int fd = open(commands.command[i].output_file, open_flags, 0644);
				if(fd == -1){
					perror("ynch");
					exit(EXIT_FAILURE);
				}
				dup2(fd, 1);
				close(fd);
			}
			if(commands.command[i].input_file != NULL){
				int fd = open(commands.command[i].input_file, O_RDONLY);
				if(fd == -1){
					perror("ynch");
					exit(EXIT_FAILURE);
				}
				dup2(fd, 0);
				close(fd);
			}
			for(int j = 0; j < n - 1; j++){
				close(pipefd[j][0]);
				close(pipefd[j][1]);
			}
			// exec here
			if(execvp(args[0], args) == -1){
				perror("yaconch");
				exit(EXIT_FAILURE);
			}
		}
		else if(pid[i] == -1){
			perror("yaconch");
		}
	}// end of forking loop
	// close all unused pipe ends
	for(int j = 0; j < n - 1; j++){
		close(pipefd[j][0]);
		close(pipefd[j][1]);
	}
	for(int i = 0; i < n; i++){
		if(!commands.background_flag){
			waitpid(pid[i], &status, WUNTRACED);
		}else{
			bjt.jobCnts++;
			printf("[%d] pid:[%jd]\n", bjt.jobCnts, (intmax_t)pid[i]);
			break;
		}
	}
	return 1;
}

// builtin function declaration
int yaconch_cd(char**);
int yaconch_help(char**);
int yaconch_exit(char**);

char *yaconch_builtin_name[] = {"cd", "help", "exit"};
int (*yaconch_builtin_func[])(char**) = {&yaconch_cd, &yaconch_help, &yaconch_exit};

int yaconch_builtin_cnt(){
	return sizeof(yaconch_builtin_name)/ sizeof(char*);
}
int yaconch_cd(char** args){
	if(args[1] == NULL){
		fprintf(stderr, "yaconch: Expected arguments to \"cd\"\n");
	}else{
		if(chdir(args[1]) != 0){
			perror("ynch");
		}
	}
	return 1;
}
int yaconch_help(char** args){
	int idx;
	printf("========================================\n");
	printf(" Yaconch Shell by Muhammad Farid Asyraf\n");
	printf(" Available commands:\n");

	for(idx = 0; idx < yaconch_builtin_cnt(); idx++){
		printf("  %s\n", yaconch_builtin_name[idx]);
	}
	printf("========================================\n");
	return 1;
}
int yaconch_exit(char** args){
	return 0;
}

int yaconch_exec(Commands command){
	int h,i;
	int n = command.commandCnts;
	for(h = 0; h < n; h++){
		char** args = command.command[h].args;
		if(args[0] == NULL){
			return 1;
		}
		for(i = 0; i < yaconch_builtin_cnt(); i++){
			if(strcmp(args[0], yaconch_builtin_name[i]) == 0){
				return(*yaconch_builtin_func[i])(args);
			}
		}
	}
	return yaconch_launch_program(command);
}
