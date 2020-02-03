#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>


struct command {
	char *args[17]; // 17th argument is buffer
	char in_file[512];
	char out_file[512];
};

typedef struct node
{
	int key; // PID
	char *value; // input in verbatim
	int isPipe; // 1 if pipe command, 0 if not
	struct node *next;
} node_t;

/* Insert at the end */
void insert(node_t *head, int PID, char *verbatim, int toggle) {
	// Go to last element
	node_t *current = head;
	while (current->next != NULL) {
		current = current->next;
	}
	
	// Add new element
	current->next = malloc(sizeof(node_t));
	current->next->key = PID;
	current->next->value = verbatim;
	current->next->isPipe = toggle;
	current->next->next = NULL;
}

/* Search based on PID and return input */
node_t* search(node_t *head, int PID) {
	node_t *current = head;
	
	while (current != NULL) {
		if (current->key == PID)
			break;
		current = current->next;
	}
	return current;
}

// remove(node_t *node, int PID)



/* Returns 1 if meta phrase, returns 0 if valid file name */
int isMeta(char *string) {
	if (string[0] == '&' || string[0] == '|' || string[0] == '&' || string[0] == '>' || string[0] == '<') {
		return 1;
	}
	return 0;
}

/* Parses single command amd stores in cmdobj */
int parse_input(char *input, struct command *cmdobj) {
	cmdobj->in_file[0] = '\0'; // Initialize
	cmdobj->out_file[0] = '\0';
	
	char *token;
	char *line = malloc(sizeof(char) * 512);
	strcpy(line, input); // To preserve input for later use
	
	// Check if input is only composed of whitespaces
	int toggle = 0;
	for(int i = 0; i < strlen(line); i++) {
		if(line[i] != ' ') {
			toggle = 1;
			break;
		}
	}
	if (toggle == 0) { // If scanning input did not find a single non-whitespace
		fprintf(stderr, "Error: invalid command line\n");
		return -1;
	}
	
	// Begin parsing
	token = strtok(line," "); // Delimiters: space and newline
	
	if (*token == '\n') { // Detection of error in first argument
		return -1;
	} else if (isMeta(token)) {
		fprintf(stderr, "Error: invalid command line\n");
		return -1;
	}
	
	for (int i = 0; i < 16; i++) { // Parse and store arguments
		cmdobj->args[i] = token;
		token = strtok(NULL," ");
	}
	
	if (token) { // If token contains something
		fprintf(stderr, "Error: too many process arguments\n");
		return -1; // Do not execute command
	}
	return 0;
}

/* Removes an element from the list of arguments in cmdobj */
void removeArgIndex(struct command *cmdobj, int i) {
	if (i == 15) { // If last element
		cmdobj->args[15] = NULL;
	} else {
		for (; i < 15; i++)
			cmdobj->args[i] = cmdobj->args[i+1];
	}
}

/* Changes stdin and stdout using dup2 if necessary */
void redirection(struct command cmdobj) {
	if (cmdobj.in_file[0] != '\0') {
		int fd = open(cmdobj.in_file, O_RDONLY);
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
	if (cmdobj.out_file[0] != '\0') {
		int fd = open(cmdobj.out_file, O_WRONLY | O_CREAT, 0644);
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
}


/* Detects input redirection and sets in_file to the file name
 * Return values:
 * -1 if file name is not provided ('Error: no input file')
 * -1 if file does not open ('Error: cannot open input file')
 * 0 if no '<' is present
 * 1: Otherwise
 */
int inputRedirection(struct command *cmdobj, char *input) {
	// Detecting input redirection '<'
	char *ch;
	char *line = malloc(sizeof(char) * strlen(input));
	strcpy(line, input); // To preserve input for later use
	
	//if (!(ch = strchr(line,'<')))
	//return 0; // If '<' not present
	
	for (int i = 0; i < 16; i++) {
		if (!cmdobj->args[i]) // if NULL, no need to continue searching
			break;
		
		if ((ch = strchr(cmdobj->args[i], '<'))) {
			char *filename = malloc(sizeof(char) * 512);
			
			// 4 possible cases   ex) toto < file
			if (*(ch+1)) { // ch is followed by something
				if (ch == cmdobj->args[i]) { // ex) toto <file
					strcpy(filename, ch+1);
					removeArgIndex(cmdobj, i);
				} else { // ex) toto<file
					strcpy(filename, ch+1);
					*ch = '\0'; // Terminate argument at '<'
				}
			} else { // Otherwise
				if (ch == cmdobj->args[i]) { // ex) toto < file
					if (cmdobj->args[i+1]) { // ex) toto < file
						strcpy(filename, cmdobj->args[i+1]);
						removeArgIndex(cmdobj, i+1);
						removeArgIndex(cmdobj, i);
					} else { // ex) toto >
						fprintf(stderr, "Error: no input file\n");
						return -1;
					}
				} else if (cmdobj->args[i+1]) { // ex) toto< file
					strcpy(filename, cmdobj->args[i+1]);
					*ch = '\0'; // Terminate argument at '<'
					removeArgIndex(cmdobj, i+1);
				} else { // ex) toto<
					fprintf(stderr, "Error: no input file\n");
					return -1;
				}
			}
			
			int fd;
			if ((fd = open(filename, O_RDONLY)) == -1) {
				fprintf(stderr, "Error: cannot open input file\n");
				return -1;
			} else {
				strcpy(cmdobj->in_file, filename); // Store filename in the object
			}
			free(filename);
		}
	}
	return 1;
}


/* Detects output redirection and sets in_file to the file name
 * Return values:
 * -1 if file name is not provided ('Error: no output file')
 * -1 if file does not open ('Error: cannot open output file')
 * 0 if no '>' is present
 * 1: Otherwise
 */
int outputRedirection(struct command *cmdobj, char *input) {
	// Detecting output redirection '>'
	char *ch;
	char *line = malloc(sizeof(char) * strlen(input));
	strcpy(line, input); // To preserve input for later use
	
	//if (!(ch = strchr(line,'>')))
	//return 0; // If '>' not present
	
	for (int i = 0; i < 16; i++) {
		if (!cmdobj->args[i]) // if NULL, no need to continue searching
			break;
		
		if ((ch = strchr(cmdobj->args[i], '>'))) {
			char *filename = malloc(sizeof(char) * 512);
			
			// Consider all possible cases ex) toto > file
			if (*(ch+1)) { // '>' is directly followed by something
				if (ch == cmdobj->args[i]) { // ex) toto >file
					strcpy(filename, ch+1);
					removeArgIndex(cmdobj, i);
				} else { // ex) toto>file
					strcpy(filename, ch+1);
					*ch = '\0'; // Terminate argument at '>'
				}
			} else { // Otherwise
				if (ch == cmdobj->args[i]) { // ex) toto > file
					if (cmdobj->args[i+1]) { // ex) toto > file
						strcpy(filename, cmdobj->args[i+1]);
						removeArgIndex(cmdobj, i+1);
						removeArgIndex(cmdobj, i);
					} else { // ex) toto >
						fprintf(stderr, "Error: no output file\n");
						return -1;
					}
				} else if (cmdobj->args[i+1]) { // ex) toto> file
					strcpy(filename, cmdobj->args[i+1]);
					*ch = '\0'; // Terminate argument at '>'
					removeArgIndex(cmdobj, i+1);
				} else { // ex) toto>
					fprintf(stderr, "Error: no output file\n");
					return -1;
				}
			}
			
			int fd;
			if ((fd = open(filename, O_WRONLY | O_CREAT, 0644)) == -1) {
				fprintf(stderr, "Error: cannot open output file\n");
				return -1;
			} else {
				strcpy(cmdobj->out_file, filename);
			}
			free(filename);
		}
	}
	return 1;
}

/* Checks if '<' is present in a string */
int isInputRed(char *line) {
	char *ch;
	if (!(ch = strchr(line,'<')))
		return 0;
	return 1;
}

/* Checks if '>' is present in a string */
int isOutputRed(char *line) {
	char *ch;
	if (!(ch = strchr(line,'>')))
		return 0;
	return 1;
}

/* Returns 0 if there is no misplaced redirection & 1 if there is */
int isMisplacedRed(char *input) {
	char *ch;
	char *line = malloc(sizeof(char) * strlen(input));
	strcpy(line, input); // To preserve input for later use
	int i = 0;
	while ((ch = strchr(line,'|'))) {
		*ch = '\0';
		
		// Input redirection only allowed in first command
		if (isInputRed(line) && i != 0)
			return 1;
		if (isOutputRed(line) != 0)
			return 1;
		
		line = ch + 1;
		i++;
	}
	
	// No input redirection < allowed in last command
	if (isInputRed(line) != 0)
		return 1;
	
	return 0;
}

/* Performs execution if an input includes a pipe
 * Returns -1 in case of error, 0 for successful execution, 1 if '|' is not present
 */
int pipeline(char *input, char *verbatim) {
	char *ch;
	char *line = malloc(sizeof(char) * strlen(input));
	strcpy(line, input); // To preserve input for later use
	struct command cmd;
	
//	if (!(ch = strchr(line,'|')))
//		return 1; // If '|' not present
	
	// If '|' present
	int i = 0;
	int in_fd = STDIN_FILENO;
	int fd[2];
	int allstatus[512];
	int pid, status;
	
	// Check if there is misplaced redirection
	if(isMisplacedRed(input) == 1) {
		fprintf(stderr, "Error: mislocated input redirection\n");
		return -1;
	}
	
	// Parse and execute command, one by one
	while ((ch = strchr(line,'|'))) {
		*ch = '\0';
		if (parse_input(line, &cmd) == -1)
			return -1;
		// Possible input redirection (in first comamnd)
		if(inputRedirection(&cmd, line) == -1)
			return -1;
		// fork - basic structure taken from main()
		pipe(fd);
		if ((pid = fork()) == 0) {
			dup2(in_fd, STDIN_FILENO);
			dup2(fd[1], STDOUT_FILENO);
			close(fd[0]);
			redirection(cmd);
			execvp(cmd.args[0], cmd.args);
			// In case of error:
			if (errno == ENOENT) { // No such file or directory
				fprintf(stderr, "Error: command not found\n");
			}
			exit(1);
		} else {
			waitpid(pid, &status, 0);
			allstatus[i] = WEXITSTATUS(status); // Store to print later
			close(fd[1]);
			in_fd = fd[0]; // Becomes next command's input
		}
		line = ch + 1;
		i++;
	}
	
	// Last command
	if (parse_input(line, &cmd) == -1)
		return -1;
	if(outputRedirection(&cmd, line) == -1) // Last command may have output redirection >
		return -1;
	
	pipe(fd);
	if ((pid = fork()) == 0) {
		dup2(in_fd, STDIN_FILENO);
		// No output redir
		close(fd[0]);
		redirection(cmd);
		execvp(cmd.args[0], cmd.args);
		// In case of error:
		if (errno == ENOENT) { // No such file or directory
			fprintf(stderr, "Error: command not found\n");
		}
		exit(1);
	} else {
		waitpid(pid, &status, 0);
		allstatus[i] = WEXITSTATUS(status);
		i++;
		close(fd[1]);
	}
	
	// Print status message
	fprintf(stderr, "+ completed '%s' ", verbatim);
	for (int j = 0; j < i; j++) {
		fprintf(stderr, "[%d]", allstatus[j]);
	}
	fprintf(stderr, "\n");
	free(input);
	free(verbatim);
	
	return 0;
}

/* Returns -1 if & is misplaced, 0 if & is not present, 1 if & is properly placed */
int isBackground(char *input) {
	char *ch;
	//char *line = malloc(sizeof(char) * strlen(input));
	//strcpy(line, input); // To preserve input for later use
	
	if (!(ch = strchr(input,'&')))
		return 0;
	
	*ch = '\0'; // Input terminates at & for proper command execution
	int toggle = 0;
	while (*(ch+1) != '\0') {
		if(*(ch+1) != ' ') {
			toggle = 1;
			break;
		}
		ch = ch + 1;
	}
	if (toggle == 1) {
		fprintf(stderr, "Error: mislocated background sign\n");
		return -1;
	}
	return 1;
}

int main(int argc, char *argv[]) {
	
	//char *cmd[3] = { "date", "-u", NULL };
	int pid, status;
	char *input, *verbatim; // input may be modified in process, verbatim does not
	node_t *head = malloc(sizeof(node_t));
	head->key = 0;
	head->value = NULL;
	head->next = NULL;
	
	while (1) {
		// Read input
		input = malloc(sizeof(char) * 512);
		verbatim = malloc(sizeof(char) * 512);
		printf("sshell$ ");
		fgets(input, 512, stdin);
		if (!isatty(STDIN_FILENO)) {
	        printf("%s", input);
	        fflush(stdout);
	    }	
		
		strtok(input, "\n"); // Remove trailing newline
		strcpy(verbatim, input);
		
		// In case of background command
		int isBgRet = isBackground(input);
		if (isBgRet == -1) // in case of error
			continue;
		
	
		// Forking in case of pipeline command
		if (strchr(input,'|')) { // If '|' is present
			if ((pid = fork()) != 0) {
				if (isBgRet == 0) { // Not a background process
					waitpid(pid, &status, 0); // Wait until the forked process dies
					free(input);
					continue;
				} else { // Process in background
					insert(head, pid, verbatim, 1); // So we can look up input based on pid later
					continue;
				}
			} else {
				// Child
				pipeline(input, verbatim);
				exit(0);
				// Exit, but not immediately, as in a successful execution of exec()
			}
		}
		
		// Parse and store input in cmdobj (single command)
		struct command cmdobj;
		if (parse_input(input, &cmdobj) == -1) { // in case of error
			continue; // error outputs are included in parse_input()
		}
		
		// Check if there is redirection
		int inRedErr = inputRedirection(&cmdobj, input);
		int outRedErr = outputRedirection(&cmdobj, input);
		
		if (inRedErr == -1) {
			continue;
		} else if (outRedErr == -1) {
			continue;
		} else if (!strcmp(cmdobj.args[0], "exit")) {
			if (waitpid(-1, &status, WNOHANG) == 0) {
				// If child process does not change its state
				fprintf(stderr, "Error: active jobs still running\n");
				fprintf(stderr, "+ completed '%s' [1]\n", verbatim);
				free(input);
				free(verbatim);
				continue;
			} else {
				// Otherwise, good to exit
				fprintf(stderr, "Bye...\n");
				exit(0);
			}
		} else if (!strcmp(cmdobj.args[0], "pwd")) {
			char cwd[1000];
			getcwd(cwd, sizeof(cwd));
			printf("%s\n", cwd);
			fprintf(stderr, "+ completed '%s' [0]\n", verbatim);
			free(input);
			free(verbatim);
			continue;
		} else if (!strcmp(cmdobj.args[0], "cd")) {
			if (chdir(cmdobj.args[1]) == -1) {
				fprintf(stderr, "Error: no such directory\n");
				fprintf(stderr, "+ completed '%s' [1]\n", verbatim);
				free(input);
				free(verbatim);
			} else {
				fprintf(stderr, "+ completed '%s' [0]\n", verbatim);
				free(input);
				free(verbatim);
			}
			continue;
		}
		
		// Forking process & execution
		if ((pid = fork()) != 0) {
			// Do not wait if background &
			if (isBgRet == 0) {
				waitpid(pid, &status, 0); // Wait until the forked process dies

			while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
				node_t *user_input = search(head, pid); // Get input based on pid
				if (!user_input->isPipe)
					fprintf(stderr, "+ completed '%s' [%d]\n", user_input->value, WEXITSTATUS(status));
				free(user_input->value);
			}
			fprintf(stderr, "+ completed '%s' [%d]\n", verbatim, WEXITSTATUS(status)); // Print to stderr
			free(input);
			free(verbatim);
			} else {
				insert(head, pid, verbatim, 0); // So we can look up input based on pid later
			}
		} else {
			// Child
			redirection(cmdobj); // Change stdin and stdout if necessary
			execvp(cmdobj.args[0], cmdobj.args);
			
			// TO BE FIXED (lec3.p3)
			// http://www.virtsync.com/c-error-codes-include-errno
			if (errno == ENOENT) { // No such file or directory
				fprintf(stderr, "Error: command not found\n");
			}
			exit(1);
		}
	}
	return EXIT_SUCCESS;
}

