/***********************************************************************************
* Anne Harris (harranne@oregonstate.edu)
* CS 344 - 400
* Program 3 - smallsh
* Description: this program implements a shell called smallsh that is similar to bash. 
* This shell allows redirection of standard input and standard output and supports both
* foreground and background processes (controllable by the command line and by recieving
* signals). This shell has three built in commands: exit, cd and status, and it supports 
* comments (lines beginning with "#")
* **********************************************************************************/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

// global variable to keep track of CTRL+Z being entered
int ignoreBack = 0;

/* catchSIGTSTP function
 * parameters: integer represeting signal number
 * returns: void
 * Function to catch the SIGTSTP signal
 * display message after foreground 
 * on first call - ignore & (blocks processes from running in background)
 * on second call - reinstate & (processes will run in background if & is used) */
void catchSIGTSTP(int signo){
	//check to see if the program is not in foreground-only mode
	if(ignoreBack == 0){
		//display message to user using write command
		char *message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
		fflush(stdout);
		//set ignore back to 1 indicating program is in foreground-only mode
		ignoreBack = 1;
	}
	//program is currently in foreground-only mode
	else{
		//display message to user
		char *message = "\nExiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 30);
		fflush(stdout);
		// set ignore back to 0 indicating program is NOT in foregournd-only mode
		ignoreBack = 0;
	}

	//reprompt user with colon
	char *prompt = ": ";
	write(STDOUT_FILENO, prompt, 2);
	fflush(stdout);
}

/* removeBackPid function
 * parameters: integer represeting the pid that completed
 * 	integer represeting the array of pids running in background
 * 	integer represeting the number of elements in the array
 * returns: void
 * Function to remove background Pid from background Pid Array when it's done completing */
void removeBackPid(int pidDone, int *pidArr, int numArr){
	int pos; //position of pid to be removed
	//find position of pid
	int i = 0;
	for(i; i < numArr; i++){
		if(pidDone == pidArr[i]){
			pos = i;
		}
	}
	//replace elements of array from that point on with the next element; 
	int j = pos;
	for(j; j < numArr -1; j++){
		pidArr[i] = pidArr[i + 1];
	}
}

/* Main driver function for program */
/* Prompts user to enter in the command line and executes the three built in commands or
 * forks() and exec() if it is not built in. Controls file redirection and background processes */
int main(int argc, char* argv[]){
	/* SET UP SIGNAL HANDLERS */

	// initialze signal stucts to be empty
	struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}; 

	// set up SIGINT action struct so the parent ignores the SIGINT signal
	SIGINT_action.sa_handler = SIG_IGN;

	// set up SIGTSTP action struct to reference the catchSIGTSTP function
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask); 	//block or delay all signals arriving
	SIGTSTP_action.sa_flags = SA_RESTART;	//automaticly restartt system calls

	//register functions
	sigaction(SIGINT, &SIGINT_action, NULL);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	//variable to represent if the program is done
	int fin = 0; //set to not be done

	//user input buffer
	char *userInput; 
	size_t bufSize = 2048;
	//allocate memory
	userInput = calloc(2048, sizeof(char));

	//delimiter is a space
	const char d[2] = " ";	//arguments will be delimited by spaces
	char *token[2048];	//declare token
	
	//have variable for the child exit method
	int childExitMethod = -5;

	//array for background PIDs
	int backPidArray[100];
	//count of pids in the background pid array
	int backPidCount = 0;

	//begin prompting the user
	while(fin == 0){

		// Variable for child pid
		pid_t spawnPid = -5; 
	
		// clear variables for file redirection 
		int input = 0;		// set to no input redirection
		int inputIndex;		//index in token array where redirection symbol occurs
		int output = 0;		// set to no output redirection
		int outputIndex;	//intdex in token array where output redirection occurs
		
		//for background processes	
		//keep track of current background pid
		int background = 0;	// set to no background symbol found
		int backExitMethod = -5;
		
		//flush stdin just incase (advice of TA)
		fflush(stdin);
	
		//prompt user using colon ":"
		printf(": ");
		fflush(stdout);

		// clear out the userInput variable and read the line in
		memset(userInput, '\0', sizeof(userInput));
		getline(&userInput, &bufSize, stdin);
	
		/* FORMAT INPUT */

		//get string length
		int len = strlen(userInput);
		
		//check for blank line
		if((len == 1) && (userInput[len-1] == '\n')){
			//set as comment so it doesn't do anything
			userInput[len-1] = '#';
		}

		//remove newline character at end of input and replace with null terminator
		if(userInput[len-1] == '\n'){
			userInput[len-1] = '\0';
		}
		
		//variable to hold if '$$' is in string meaning we need to replace with pid
		int pidThere = 0;
		//string to find $$
		const char needle[2] = "$$";
		//test to see if '$$' is in string
		char *test = strstr(userInput, needle);
		if(test != NULL){	//$$ was found
			pidThere = 1; 
		}
		
		//varaible to hold the final user input buffer
		char *finalBuff;
		finalBuff = calloc(2048, sizeof(char));
		
		//if there's a pid expansion needed
		//Cite: addapted from sample code found in stack overflow article titled: "what is the function to
		//replace string in C?" (6th response)
		//"https://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
		if(pidThere == 1){
			char *orgptr; //pointer to point to original user input
			char *patloc; //location of the $$ pattern
			int patCnt;	//number of times $$ pattern occurs
			char replacement[2] = "%d"; //repalce $$ with %d

			//get number of times pattern occurs
			//set pointer to user input, set the patter location to use strstr, increase pointer by 2 (length of $$)
			for(orgptr = userInput; patloc = strstr(orgptr, needle); orgptr = patloc + 2){
				patCnt++;
			}
		
			//replacement count
			int repCnt = 0;
			//create and populate new buffer with replacements '%d'
			char * newBuff = calloc(2048, sizeof(char)); //string used to replace $$ with %d
			if(newBuff != NULL){
				//set new buffer pointer to new buffer string
				char * newBuffPtr = newBuff;
				//set pointer to user input, while the pattern location is equl; increase pointer
				for(orgptr = userInput; patloc = strstr(orgptr, needle); orgptr = patloc + 2){
					size_t skplen = patloc - orgptr; //skip length
					strncpy(newBuffPtr, orgptr, skplen); //copy section up to pattern
					newBuffPtr += skplen; 
					strncpy(newBuffPtr, replacement, 2); //copy %d in
					repCnt++;		//increase replacement count
					newBuffPtr += 2;	//move new buffer pointer
				}
			}

			//replace instances of %d with getpid and copy into final buffer
			int y = 0;
			for(y; y < repCnt; y++){
				sprintf(finalBuff, newBuff, getpid());
			}
			//free memory of new buffer
			free(newBuff);
		}
		//no pid expansion, just copy over user input
		else{
			strcpy(finalBuff, userInput);
		}
		
		// get parsed arguments
		//get first token
		token[0] = strtok(finalBuff, d);
		//store all arguments in token array
		int numArgs = 0;
		while(token[numArgs] != NULL){
			numArgs++; //increment to next spot in array
			token[numArgs] = strtok(NULL, d); //set array to token
			token[numArgs+1] = NULL; //set next spot in array to NULL
			//check for file redirection
			//input redirection
			if(strcmp(token[numArgs-1], "<") == 0){
				input = 1;
				inputIndex = numArgs-1;
			}
			//output redirection
			if(strcmp(token[numArgs-1], ">") == 0){
				output = 1;
				outputIndex = numArgs-1;
			}
		}
	
		//check for background process in last spot
		if(strcmp(token[numArgs-1], "&") == 0){
			//check to see if SIGTSTP (CTRL-Z) has been set
			if(ignoreBack == 1){
				//don't set background flag
				background = 0;
				//remove token so it doesn't cause any issue
				token[numArgs-1] = '\0';
			}
			else{
				// set background flag 
				background = 1;
			}
		}

		/* CHECK FOR BUILT IN COMMANDS */
		//built in commands run from parent shell

		//handle exit command
		//exit command exits shell and kills all other processes or jobs
		if(strncmp(token[0], "exit", 4)== 0){
			//kill any background processes
			int j=0;
			for(j; j < backPidCount; j++){
				kill(backPidArray[j], SIGKILL);
			}
			//exit program
			fin = 1;	//set while loop checker to 1 just in case
			return 0;
		}		

		//handle cd command
		else if(strncmp(token[0], "cd", 2)== 0){
			//check to see if path is specified
			if(token[1] != NULL){
				//change to specified directory
				int change = chdir(token[1]);
				if(change == -1){
					printf("Directory does not exist\n");
					fflush(stdout);
				}
			}
			//no path specified
			else{
				//change to home directory
				chdir(getenv("HOME"));
			}
		}

		//handle status command
		else if(strncmp(token[0], "status", 6)== 0){
			//call waitpid to get status, with NOHANG
			waitpid(-1, &childExitMethod, WNOHANG);
			if(WIFEXITED(childExitMethod)){
				int exitStatus = WEXITSTATUS(childExitMethod);
				printf("exit value %d\n", exitStatus);
				fflush(stdout);
			}
			else if(WIFSIGNALED(childExitMethod) != 0){
				//signal status
				int termSignal = WTERMSIG(childExitMethod);
				printf("terminated by signal %d\n", termSignal);
				fflush(stdout);
			}
		}
	
		//handle comment (#)
		else if(strncmp(token[0], "#", 1)==0){
			//comment or blank line, do nothing
		}
		//non built in command
		else{
			//fork into child

			//redirector variables	
			int sourceFD, targetFD, result;
			
			//check for file redirectors
			//there is either input or outbut but it is NOT a background process
			if((input == 1 || output == 1) && background == 0){
				
				//fork
				spawnPid = fork();
				
				// there is both input and output
				if(input == 1 && output == 1){
					//fork was successful
					if(spawnPid == 0){
						//reinstate SIGINT to default
						SIGINT_action.sa_handler = SIG_DFL;
						sigaction(SIGINT, &SIGINT_action, NULL);
						
						//printf("(source open)token[inputIndex + 1] = %s\n", token[inputIndex +1]);
						//fflush(stdout);
						//open file and redirect source
						sourceFD = open(token[inputIndex +1], O_RDONLY);
						if(sourceFD == -1){perror(""); exit(1);}
						result = dup2(sourceFD, 0);
						if(result == -1){ perror(""); exit(2);}
						//close file on exec
						fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
						
						//printf("(target open)token[outputIndex +1 = %s\n", token[outputIndex +1]);
						//fflush(stdout);
						//redirect target
						targetFD = open(token[outputIndex +1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if(targetFD == -1){perror(""); exit(1);}
						result = dup2(targetFD, 1);
						if(result == -1){ perror(""); exit(2);}
						//close file
						fcntl(targetFD, F_SETFD, FD_CLOEXEC);
						//run exec
						//printf("(passed)token[inputIndex -1] = %s\n", token[inputIndex-1]);
						//fflush(stdout);
						execlp(token[inputIndex-1], token[inputIndex-1], NULL);
						exit(1);
					}
				}
			
				//input redirection only
				else if(input == 1 && output == 0){
					if(spawnPid == 0){
						//reinstate SIGINT to default
						SIGINT_action.sa_handler = SIG_DFL;
						sigaction(SIGINT, &SIGINT_action, NULL);
						
						//printf("(passed)token[inputIndex - 1 = %s\n", token[inputIndex-1]);
						//fflush(stdout);
						//open file
						//printf("(open)token[inputIndex+1] = %s\n", token[inputIndex+1]);
						//fflush(stdout);
						sourceFD = open(token[inputIndex + 1], O_RDONLY);
						if(sourceFD == -1){ perror(""); exit(1);}
					
						//redirect stdin to sourceFD
						result = dup2(sourceFD, 0);
						if(result == -1){ perror(""); exit(2);}
						
						// close file on exec
						fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
						execlp(token[inputIndex-1], token[inputIndex -1], NULL);
						exit(1);
					}
				}
				//output redirection only
				else if(output == 1 && input == 0){
					if(spawnPid == 0){
						//printf("(passed)token[outPutIndex-1] = %s\n", token[outputIndex -1]);
						//fflush(stdout);
						
						//reinstate SIGINT to default
						SIGINT_action.sa_handler = SIG_DFL;
						sigaction(SIGINT, &SIGINT_action, NULL);
						
						//open file
						//printf("(open)token[outputIndex+1] = %s\n", token[outputIndex+1]);
						//fflush(stdout);
						targetFD = open(token[outputIndex+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if(targetFD == -1){ perror(""); exit(1);}
						
						//redirect stdout to targetFD
						result = dup2(targetFD, 1);
						if(result == -1){ perror(""); exit(2);}
						
						//close file on exec
						fcntl(targetFD, F_SETFD, FD_CLOEXEC);	
						
						//execlp to write to file
						execlp(token[outputIndex-1], token[outputIndex - 1], NULL);
						exit(1);
					}
				}
				//wait on child process to finish before going forward
				waitpid(spawnPid, &childExitMethod, 0);
			}

			//start process in background
			else if(background == 1){
				//replace & with NULL
				token[numArgs - 1] = NULL;
	
				pid_t backPid = -5;	
				//fork child
				backPid = fork();
			
				//fork was successful				
				if(backPid == 0){
				//	printf("in background child %d\n", getpid());
				//	fflush(stdout);
					
					//check for redirection
					//no input or output specified, redirect to /dev/null
					if(input == 0 && output == 0){
						int devNull = open("/dev/null", O_WRONLY | O_RDONLY | O_CREAT | O_TRUNC);
						dup2(devNull, 0);
						dup2(devNull, 1);
					
						//exec	
						execvp(token[0], token);
						perror("");
						exit(2); break;
					}
					
					//both input and output specified
					else if(input == 1 && output == 1){
						//open file and redirect source
						sourceFD = open(token[inputIndex +1], O_RDONLY);
						if(sourceFD == -1){perror(""); exit(1);}
						result = dup2(sourceFD, 0);
						if(result == -1){ perror(""); exit(2);}
						//close file on exec
						fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
						
						//redirect target
						targetFD = open(token[outputIndex +1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if(targetFD == -1){perror(""); exit(1);}
						result = dup2(targetFD, 1);
						if(result == -1){ perror(""); exit(2);}
						//close file
						fcntl(targetFD, F_SETFD, FD_CLOEXEC);
						//run exec
						execlp(token[inputIndex-1], token[inputIndex-1], NULL);
						exit(1);
					}
			
					//only input redirection
					else if(input == 1 && output == 0){
						//printf("token[inputIndex + 1 = %s\n", token[inputIndex+1]);
						//fflush(stdout);
						//open file
						sourceFD = open(token[inputIndex + 1], O_RDONLY);
						if(sourceFD == -1){ perror(""); exit(1);}
					
						//redirect stdin to sourceFD
						result = dup2(sourceFD, 0);
						if(result == -1){ perror(""); exit(2);}
						
						// close file on exec
						fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
						execlp(token[inputIndex-1], token[inputIndex -1], NULL);
						exit(1);
					}
					//only output redirection
					else if(output == 1 && input == 0){
						//open file
						targetFD = open(token[outputIndex+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if(targetFD == -1){ perror(""); exit(1);}
						
						//redirect stdout to targetFD
						result = dup2(targetFD, 1);
						if(result == -1){ perror(""); exit(2);}
						
						//close file on exec
						fcntl(targetFD, F_SETFD, FD_CLOEXEC);	
						
						//execlp to write to file
						execlp(token[outputIndex-1], token[outputIndex - 1], NULL);
						exit(1);
					}
				}			
				// PUT BACKGROUND PID IN ARRAY
				backPidArray[backPidCount] = backPid;
				//increase array count
				backPidCount++;
				
				//print background pid to screen
				printf("background pid is %d\n", backPid);
				fflush(stdout);
	
			}
			//foreground
			else{
				//fork child
				spawnPid = fork();
				
				switch(spawnPid){
					//error
					case -1: { perror("Fork failed\n"); exit(1); break;}
					//child
					case 0: {
						//reinstate SIGINT to default
						SIGINT_action.sa_handler = SIG_DFL;
						sigaction(SIGINT, &SIGINT_action, NULL);
						
						//printf("CHILD(%d): executing\n", getpid());
						//fflush(stdout);
						execvp(token[0], token);
						perror("");
						exit(2); break;
					}
					//parent
					default: {
						pid_t actualPid = waitpid(spawnPid, &childExitMethod, 0);
						break;
					}
				}
			}
		}
		
		//check background pids to see if they've been completed
		int c = 0;
		for(c; c < backPidCount; c++){
			//call wait, this will check all pids for each pid in the array
			//so if pid at index 0 completes by the time it's checking the pid
			//at index c it can be displayed
			pid_t backPid_actual = waitpid(-1, &backExitMethod, WNOHANG);
			if(backPid_actual != 0){
				//check for exit status
				if(WIFEXITED(backExitMethod)){
					int backExitStatus = WEXITSTATUS(backExitMethod);
					printf("background pid %d is done: exit value %d\n", backPid_actual, backExitStatus);
					fflush(stdout);
					//remove the pid from the back pid array, as it's not longer needed to check
					removeBackPid(backPid_actual, backPidArray, backPidCount);
					backPidCount--;
				}
				//if no exit status, check for signal
				else if(WIFSIGNALED(backExitMethod)){
					int backTermSignal = WTERMSIG(backExitMethod);
					printf("background pid %d is done: terminated by signal %d\n", backPid_actual, backTermSignal);
					fflush(stdout);
					//remove pid from back pid array
					removeBackPid(backPid_actual, backPidArray, backPidCount);
					backPidCount--;
				}
			}
		}
		// free memory
		free(finalBuff);
	}

	//free memory
	free(userInput);

	return 0;
}
