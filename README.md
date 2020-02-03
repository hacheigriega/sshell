+ ## Phase 1: Running Commands the Hard Way
  For this phase, we simply implemented the fork() + exec() + wait() method to      
  have the forked child execute the desired command and the parent wait   
  for the child to finish. This way, our shell stays alive even if a command   
  execution results in a failure. 
  ```
   pid = fork ();
   if (pid == 0) {
	//Child
	execvp(cmd[0], cmd);
	perror("execvp");
    	exit(1);
   } else if (pid > 0) {
        // Parent
        waitpid(-1, &status, 0);
        printf("+ completed '/bin/date -u' [%d]\n", WEXITSTATUS(status));
   } else {
           perror("fork");
           exit(1);
   }
  ```
+ ### Phase 2: Read Commands from Input
  To make our shell interactive, we used a while loop with the condition that   
  only the parent process passes. Taking advantage of the constraint on the  
  command length, we allocated enough memory before reading in the command  
  line. This was stored in a pointer array called "cmd." Then, execvp()  
  executed the appropriate command as before.
  ```
  pid = 1;
  while (pid > 0) {
	cmd[0] = malloc(sizeof(char) * 512);
	cmd[1] = NULL;
	printf("sshell$ ");
	scanf("%s", cmd[0]);
	...
  ```
+ ### Phase 3: Arguments
  To take into account arguments, we parsed the command line, tokenizing and  
  storing each argument in an array. This array of 16 elements was part of a  
  struct called "command."  
  ```
  struct command {
	  char *args[16];
  };

  struct command parse_input(char *input) {
	  char *token;
	  struct command cmdobj = {NULL};
	
	    token = strtok(input," \n"); // Delimiters: space and newline
	    for (int i = 0; i < 16; i++) {
		    cmdobj.args[i] = token;
		    token = strtok(NULL," \n");
	    }
	    return cmdobj;
  }
  ```
+ ### Phase 4: Builtin Commands
  To satisfy this phase, we used a series of simple conditional statements and  
  implemented corresponding command executions. For instance, the exit command  
  was implemented as follows:
  ```
  	...
	else if (!strcmp(cmdobj.args[0], "exit")) {
		fprintf(stderr, "Bye...\n");
		exit(0);
	}
	...
  ```
+ ### Phase 5: Input Redirection
  In the parsing stage, to work around the issue of reading the different  
  possibilities of placement of the "<" character, 4 cases were considered:  
  + Case 1: cmd <file
  + Case 2: cmd<file
  + Case 3: cmd < file
  + Case 4: cmd< file  
  After detecting a redirection and storing the file name, we made sure to trim  
  or remove arguments so that execvp() can properly run the command. The following  
  code snippet shows a part of our implementation:  
  ```
  if (*(ch+1)) { // ch is followed by something
		if (ch == cmdobj->args[i]) { // ex) toto <file
		strcpy(filename, ch+1);
		removeArgIndex(cmdobj, i);
  }
  else { // ex. toto<file
		strcpy(filename, ch+1);
		*ch = '\0'; // Terminate argument at '<'
  }
  ```
  
  In the forked child, immediately before execvp() is run, we used  
  dup2() based on the stored information from the parsing stage to modify the  
  default input or output source.  
    
  The error management was divided into two parts: parsing error detection and  
  execution error detection. The parsing stage took care of issues such as   
  misplacements or invalid file names. We determined whether a string is a file  
  name based on a simple check of the first character, as shown below:  
  ```
  int isMeta(char *string) {
	  if (string[0] == '&' || string[0] == '|' || string[0] == '&' ||  
          string[0] == '>' || string[0] == '<') { 
		  return 1;
	  }
	  return 0;
  } // Returns 1 if meta phrase, returns 0 if valid file name
  ```
  If there was a parsing error, we ensured that the command is not executed.  
  On the other hand, identifying execution errors involved dealing with errno  
  in the parent process.  

+ ### Phase 6: Output Redirection
  Output Redirection was handled in the exact same manner as input redirection.  

+ ### Phase 7: Pipeline Commands
  Implementing pipeline posed a challenge because it involves processing of  
  multiple commands. Therefore, we created a separate function that dealt with  
  command lines containing a pipe character. Instead of storing a series of   
  commands first then executing them, we parsed and executed the commands   
  one by one in a loop. We used pipe() and dup2() to allow communication  
  between consecutive processes.
  ```
  while ((ch = strchr(line,'|'))) {
		*ch = '\0';
		if (parse_input(line, &cmd) == -1) // Parse
			return -1;
		...
		pipe(fd); 
		// fork - basic structure taken from main()
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
  ```
  
+ ### Phase 8: 
  To determine whether or not a command would be run in the background, it  
  was first checked to see that the '&' character was the last one in a  
  command. As always, a parse error such as a misplacement of '&' resulted  
  in an error that skips the current loop iteration immediately. A properly  
  placed '&' would set an indicator variable ```isBegRet``` so that the  
  parent does not wait for the completion of the child process. Instead,  
  the parent, or our shell, stores the information about the command in a  
  linked list. Our shell regularly checks if a child process has returned.  
  If it does, it looks up the linked list with the PID as the key to find  
  the original command line to print. 

```
while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	node_t *user_input = search(head, pid); // Get input based on pid  
	if (!user_input->isPipe) // Skip if the returned process was a pipe command  
		fprintf(stderr, "+ completed '%s' [%d]\n", user_input->value, WEXITSTATUS(status));  
	free(user_input->value);
}
```
  A limitation of our program becomes evident here. Since we implemented  
  pipeline as a separate function, the print mechanism is different for  
  the commands involving a pipe. That is, for pipe commands, the printing  
  of the status is included in the process itself, unlike a regular command.  
  This inconsistency forces us to add a condition check that only prints  
  the completion status if a command did not involve a pipe. Also, a combination  
  of pipeline and background sometimes does not exit properly. However, it seems  
  that even the reference program is at times flawed in this regard. This  
  inconsistency could have been avoided if we had sketched out the plan in more  
  detail.

___


## Sources:
+ https://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html
+ https://brandonwamboldt.ca/how-bash-redirection-works-under-the-hood-1512
+ https://guides.github.com/features/mastering-markdown/
+ https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet
+ https://gist.github.com/mplewis/5279108
+ http://www.zentut.com/c-tutorial/c-linked-list/ 
+ http://www.virtsync.com/c-error-codes-include-errno
