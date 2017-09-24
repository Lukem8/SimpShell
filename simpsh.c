#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>

// Create struct to use with wait command
struct subProc
{
	pid_t pid;
	char **args;
};

// Globals
int numProcesses;
int totalProcesses;
struct subProc *pinfo;
int fdCount;
int *fileDescriptors;
int *pipes;
int pipeCount;
int BUFFSIZE;
int verboseFlag;
int waitOption;
int _stdin;
int _stdout;
int _stderr;
int profileFlag;

// In parent process
void waiting(){
    int i,j, status;
    int finish;
    for(i = 0; i < numProcesses; i++){
      finish = waitpid(-1, &status, 0);
      printf("Exit status/pid of child process is %d\n", finish);

      for(j = 0; j < numProcesses; j++){
        if (finish == pinfo[j].pid)
          break;
      }
      char* p;
      int k = 0;
      while(pinfo[j].args[k] != NULL){
        p = pinfo[j].args[k];
        printf("%s ", p);
        k++;
      }
      printf("\n" );
    }
}

// Function to execute command given by --command option
void executeCommand(char** arguments, int* fileDescriptors) {
  // arguments holds the three file descriptors,command,any additional arguments
  // fileDescriptors holds the fileDescriptors passed in through input
  int i = 0;
  int status;

  char index = **arguments; //get first argument
  char* arg1= *arguments; // Gets stdin file descriptor index
  char* arg2 = *(arguments+1); // Gets stdout file descriptor index
  char* arg3 = *(arguments+2);  // Gets stderr file descriptor index
  char** p = (arguments+3); // point to the executable
  char * cmd = *p;
  char** args = (char**)malloc(sizeof(char*) * 100);
  i = 0;
  int count = -1;
  p++;
  while(*p){
    args[i] = *p;
    p++;
    i++;
    count++;
  }
  args[i] = NULL;

  // Check for verbose flag and print out command with args to stdout
  if(verboseFlag){
    printf("Printing from --verbose option: --command %s %s %s %s\n",
     arg1, arg2, arg3, cmd);
    i=0;
    while(args[i]){
      printf("Printing arguments from --verbose option: %s\n", args[i]);
      i++;
    }
  }

  //fork a new process
  int f = fork();
  if (f < 0){
    perror("Fork Failed! Exiting program.");
    exit(3);
  }
	int pipe1=0, pipe2 = 0, pipe3 = 0;
  // Child process
  if (f == 0){

      // setup IO redirection (Housekeeping for new child process)
			if(fileDescriptors[*arg1-48]){

        close(0);
        dup(fileDescriptors[*arg1-48]);
      }
			else if(pipes[*arg1-48]){

        dup2(pipes[*arg1-48],0);
				pipe1 = 1;
				close(pipes[*arg1-48]);
			}
      else{
        fprintf(stderr, "Invalid File descriptor!!!\n" );
        exit(4);
      }


      if(fileDescriptors[*arg2-48]){
        close(1);
        dup(fileDescriptors[*arg2-48]);
      }
			else if(pipes[*arg2-48]){
        dup2(pipes[*arg2-48],1);
				pipe2=1;

				close(pipes[*arg2-48]);
			}
      else{
        fprintf(stderr, "Invalid File descriptor!!!\n" );
        exit(4);
      }
      if(fileDescriptors[*arg3-48]){
        close(2);
        dup(fileDescriptors[*arg3-48]);
      }
			else if(pipes[*arg3-48]){
				pipe3=1;
        dup2(pipes[*arg3-48],2);
				//close(pipes[*arg3-48-1]);	// close read end
			}
      else{
        fprintf(stderr, "Invalid File descriptor!!!\n" );
        exit(4);
      }

			pipe1 = pipe2=pipe3=0;

			// Unknown reason causing sleep command to fail with execvp()
			if (!strcmp(cmd, "sleep")){
				execlp(cmd, cmd, args[0], NULL);
			}
			if (!strcmp(cmd, "grep")){
				execlp(cmd, cmd, args[0], NULL);
			}
			if (!strcmp(cmd, "sed")){
				execlp(cmd, cmd, args[0], NULL);
			}
      // Execute the Command
      if (args[0]==NULL){
        execlp(cmd, cmd, NULL);
      }
      else{
        execvp(cmd, args);
      }
  }
	else {	// Parent process
		pinfo[numProcesses].args = (char **) malloc(totalProcesses * sizeof (char *));
		pinfo[numProcesses].pid = f;	// get pid
		close(pipes[*arg2-48]);
		p = (arguments+3); // point at the command
		i=0;
		while(*p){
			pinfo[numProcesses].args[i] = *p;
			p++;
			i++;
		}
		numProcesses++;
	}
  free(args);
  return;
}

// Signal handler function
void my_handler(int num){
  fprintf(stderr, "signal number %d caught\n", num);
	exit(num);
}

// Function to cause segfault
void crash(){
  int* a = NULL;
  *a = 3;
}

// Print out resources used by calling process
void profile_print() {

	printf("\n\t\t---------- Printing getrusage information from --profile option ----------\n" );
	struct rusage usage;
	int ret = getrusage(RUSAGE_SELF, &usage);
	if (ret < 0){
		fprintf(stderr, "Error with getrusage\n" );
	}
	printf("\t\tUser CPU time used:\t\t%lld seconds \t%lld microseconds\n", (long long) usage.ru_utime.tv_sec, (long long) usage.ru_utime.tv_usec);
	printf("\t\tSystem CPU time used:\t\t%lld seconds \t%lld microseconds\n", (long long) usage.ru_stime.tv_sec,  (long long) usage.ru_stime.tv_usec);
	printf("\t\tMax resident set size:\t\t%ld\n", usage.ru_maxrss);
	printf("\t\tSoft page faults:\t\t%ld\n", usage.ru_minflt);
	printf("\t\tHard page faults:\t\t%ld\n", usage.ru_majflt);
	printf("\t\tVoluntary context switches:\t%ld\n", usage.ru_nvcsw);
	printf("\t\tInvoluntary context switches:\t%ld\n", usage.ru_nivcsw);

	return;
}

// Open a single pipe
void open_pipe() {
	int pipefd[2];
	if (pipe(pipefd) == -1) {
      perror("Error with pipe");
			exit(EXIT_FAILURE);
  }
	if (pipeCount+fdCount >= BUFFSIZE){
		BUFFSIZE += BUFFSIZE;
		pipes = realloc(pipes, BUFFSIZE);
		if (!pipes){
			fprintf(stderr, "Error allocating memory. Terminating program.\n" );
			exit(2);
		}
	}
	pipes[fdCount] = pipefd[0];
	fdCount++;
	pipes[fdCount] = pipefd[1];
	fdCount++;
	pipeCount += 2;
	return;
}


extern int opterr;

int main(int argc, char **argv) {

  // Save stdin stdout and stderr for later
  _stdin = dup(0);
  _stdout = dup(1);
  _stderr = dup(2);
  char* x;
  int c,index, j = 0, i = 0;
  int ifd, signalNum;
  fdCount = 0;
  BUFFSIZE = 100;
  verboseFlag = 0;
  waitOption = 0;
	profileFlag = 0;
  //setup for subprocesses with --wait command
  numProcesses = 0;
  totalProcesses = 20;
  pinfo = (struct subProc * )malloc( sizeof(struct subProc) *totalProcesses );
  if (!pinfo){
    fprintf(stderr, "Error allocating memory. Terminating program.\n");
    exit(2);
  }

  // Array of file descriptors
  fileDescriptors = malloc(sizeof(int)*BUFFSIZE);
	pipes = malloc(sizeof(int)*BUFFSIZE);
	pipeCount = 0;


  if (!fileDescriptors){
    fprintf(stderr, "Error allocating memory. Terminating program.\n");
    exit(2);
  }

  int oflags = 0;
  // Create argument options for getopt_long
  static struct option options[] = {
            {"rdonly",  required_argument, NULL, 'r' },
            {"wronly",  required_argument, NULL, 'w' },
            {"command", required_argument, NULL, 'c' },
            {"verbose", no_argument, NULL, 'v' },
            {"wait", no_argument, NULL, 'z'},
            {"rdwr", required_argument, NULL, 'q'},
            {"close", required_argument, NULL, 'k'},
            {"abort", no_argument, NULL, 'x'},
            {"catch", required_argument, NULL, 'y'},
            {"ignore", required_argument, NULL, 'i'},
            {"default", required_argument, NULL, 'd'},
            {"pause", no_argument, NULL, 'p'},
            {"append", no_argument, NULL, O_APPEND},
            {"cloexec", no_argument, NULL, O_CLOEXEC},
            {"creat", no_argument, NULL, O_CREAT},
            {"directory", no_argument, NULL, O_DIRECTORY},
            {"dsync", no_argument, NULL, O_DSYNC},
            {"excl", no_argument, NULL, O_EXCL},
            {"nofollow", no_argument, NULL, O_NOFOLLOW},
            {"nonblock", no_argument, NULL, O_NONBLOCK},
            {"rsync", no_argument, NULL, O_RSYNC},
            {"sync", no_argument, NULL, O_SYNC},
            {"trunc", no_argument, NULL, O_TRUNC},
						{"pipe", no_argument, NULL, 's'},
						{"profile", no_argument, NULL, 'm'},
            {NULL, 0, NULL, 0},
  };

  while((c = getopt_long(argc, argv, "r:w:c:v", options, NULL)) != -1){
    opterr = 0;
     switch(c){

			case 'm':
				profileFlag = 1;
			 	break;
			case 's':
				if (verboseFlag)
					printf("--pipe\n");
				// Open a pipe, consumes 2 file numbers
				open_pipe();
				if (profileFlag)
					profile_print();
				break;
      case O_APPEND:
 			case O_CLOEXEC:
      case O_CREAT:
 			case O_DIRECTORY:
 			case O_DSYNC:
 			case O_NOFOLLOW:
 			case O_EXCL:
 			case O_NONBLOCK:
 			case O_SYNC:
 			case O_TRUNC:
        if (verboseFlag)
          printf("%s ", argv[optind - 1]);
          oflags |= c;  // add the current flag to the list
          break;
      case 'k':
        x = optarg;
        int num = atoi(x);
        // Close the num indexed fileDescriptor then make that spot in Array 0
				if(fileDescriptors[num]){
        	close(fileDescriptors[num]);
        	fileDescriptors[num] = 0;
				}
				else{
					close(pipes[num]);
        	pipes[num] = 0;
				}
				if (profileFlag)
					profile_print();
        break;
      case 'q':
        ifd = open(optarg, O_RDWR | oflags);
        if (ifd < 0) {
          fprintf(stderr, "Error occurred opening file.\n");
          exit(1);
        }
        if (ifd >= 0) {
           if(fdCount >= BUFFSIZE){
             BUFFSIZE += BUFFSIZE;
             fileDescriptors = realloc(fileDescriptors, BUFFSIZE);
             if (!fileDescriptors){
               fprintf(stderr, "Error allocating memory. Terminating program.\n" );
               exit(2);
             }
           }
           fileDescriptors[fdCount] = ifd;
           fdCount++;
           oflags = 0;
         }

				if (profileFlag)
					profile_print();
        break;
      case 'r':
        ifd = open(optarg, O_RDONLY | oflags);
        if (ifd < 0) {
          fprintf(stderr, "Error occurred opening file.\n");
          exit(1);
        }
        if (ifd >= 0) {
           if(fdCount >= BUFFSIZE){
             BUFFSIZE += BUFFSIZE;
             fileDescriptors = realloc(fileDescriptors, BUFFSIZE);
             if (!fileDescriptors){
               fprintf(stderr, "Error allocating memory. Terminating program.\n" );
               exit(2);
             }
           }
           fileDescriptors[fdCount] = ifd;
           fdCount++;
           oflags = 0;
         }
				if (profileFlag)
					profile_print();
        break;
      case 'w':
      	ifd = open(optarg, O_WRONLY | oflags);
      	if (ifd < 0) {
        	fprintf(stderr, "Error occurred opening file.\n");
        	exit(1);
      	}
      	if (ifd >= 0) {
         if(fdCount >= BUFFSIZE){
           BUFFSIZE += BUFFSIZE;
           fileDescriptors = realloc(fileDescriptors, BUFFSIZE);
         }
       	}

       	fileDescriptors[fdCount] = ifd;
       	fdCount++;
       	oflags = 0;
			 	if (profileFlag)
					profile_print();
        break;
      // Case to execute commands
      case 'c':
        index = optind-1;
        int commandCounter = 0;
				 /* buffer to hold arguments for the current command */
        char** argbuf = malloc(sizeof(char)*BUFFSIZE);
        i = 0;
        while (index < argc && strcmp((char*)argv[index], "--wait") ) {
					// if the argument contains "--" then dont add to list and break from loop
          if(strstr((char*)argv[index], "--") != NULL) {
            break;
          }
          argbuf[i] = (char*)argv[index];
          commandCounter++;
          index++;
          i++;
        }

        executeCommand(argbuf, fileDescriptors);
        for (i = 0; i < commandCounter; i++){
          argbuf[i] = NULL;
          free(argbuf[i]);
        }
        free(argbuf);
				if (profileFlag)
					profile_print();
        break;
      case 'v':
        verboseFlag = 1;
        break;
      case 'z':
			if (verboseFlag)
				printf("--wait\n");

				// close all opened file descriptors
				for(i = 0; i < fdCount; i++){
					if (fileDescriptors[fdCount])
						close(fileDescriptors[fdCount]);
				}
				for (i = 0; i < fdCount; i++){
					if (pipes[fdCount]){
						close(pipes[fdCount]);
						pipes[fdCount] = 0;
					}
				}
				waiting();
        waitOption = 1;
				if (profileFlag){

				printf("\n\t\tPrinting getrusage info for Parent process\n" );
				profile_print();
				struct rusage usage;
				int ret = getrusage(RUSAGE_CHILDREN, &usage);
				if (ret < 0){
					fprintf(stderr, "Error with getrusage\n" );
				}
				printf("\t\tPrinting getrusage info for Children processes\n" );
				printf("\t\tUser CPU time used:\t\t%lld seconds \t%lld microseconds\n", (long long) usage.ru_utime.tv_sec, (long long) usage.ru_utime.tv_usec);
				printf("\t\tSystem CPU time used:\t\t%lld seconds \t%lld microseconds\n", (long long) usage.ru_stime.tv_sec,  (long long) usage.ru_stime.tv_usec);
				printf("\t\tMax resident set size:\t\t%ld\n", usage.ru_maxrss);
				printf("\t\tSoft page faults:\t\t%ld\n", usage.ru_minflt);
				printf("\t\tHard page faults:\t\t%ld\n", usage.ru_majflt);
				printf("\t\tVoluntary context switches:\t%ld\n", usage.ru_nvcsw);
				printf("\t\tInvoluntary context switches:\t%ld\n", usage.ru_nivcsw);
			}
        break;

      case 'x':
        crash();
				if (profileFlag)
					profile_print();
        break;
      case 'y':
        signalNum = atoi(optarg);
        signal(signalNum, my_handler);
				if (profileFlag)
					profile_print();
        break;
      case 'i':
        signalNum = atoi(optarg);
        if (verboseFlag)
					printf("--ignore %d\n", signalNum);
        signal(signalNum, SIG_IGN);
				if (profileFlag)
					profile_print();
        break;
      case 'd':
        signalNum = atoi(optarg);
        if (verboseFlag)
					printf("--default %d\n", signalNum);
        signal(signalNum, SIG_DFL);
				if (profileFlag)
					profile_print();
        break;
      case 'p':
        pause();
				if (profileFlag)
					profile_print();
        break;
      case '?':
        fprintf(stderr, "%s: option '-%c' is invalid: ignored\n", argv[0], optopt);
        break;
     }
  }

// Free dynamically allocated memory
for (i = 0; i < numProcesses; i++){
	free(pinfo[numProcesses].args);
}
free(pipes);
free(pinfo);
free(fileDescriptors);
exit(0);
}
