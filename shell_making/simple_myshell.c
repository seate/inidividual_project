#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_CMD_ARG 20
#define CMD_BUFSIZ 256

const char *prompt = "myshell> ";
char* cmdvector[MAX_CMD_ARG];
char  cmdline[CMD_BUFSIZ];
char *betweenPipe[MAX_CMD_ARG];
struct sigaction act;

void fatal(char *str){
	perror(str);
	exit(1);
}

int makelist(char *s, const char *delimiters, char** list, int MAX_LIST){ //명령어 분리해서 return
	int i = 0, numtokens = 0;
	char *snew = NULL;

	if( (s == NULL) || (delimiters == NULL) ) return -1;

	snew = s + strspn(s, delimiters);	/* Skip delimiters */ //처음 공백 삭제?
	if( (list[numtokens] = strtok(snew, delimiters)) == NULL ) return numtokens; // 없을 때 0 return
	numtokens = 1;

	while(1){
		if( (list[numtokens] = strtok(NULL, delimiters)) == NULL) break; //다 리스트로 만들었으면 break
		if(numtokens == (MAX_LIST-1)) return -1; //꽉 찼는데도 남았을 때 -1 return
		numtokens++; //개수
	}

	return numtokens; //개수 return
}

void _handler(int nope){ //아무것도 하지 않음
	printf("\n");
	return;
}

void chldcollector(){ //좀비 프로세스 회수
	while (waitpid(-1, NULL, WNOHANG) > 0){}
}

void signalSetting(){ //시그널 핸들러 지정
	act.sa_handler = _handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGTSTP, &act, NULL);
	signal(SIGCHLD, chldcollector);
}

void signalSettingClear(){ //자식 프로세스에서 시그널 핸들러 초기화
	act.sa_handler = SIG_DFL;
}


void _redirection(int isIn){
	char *truncate = isIn ? "<" : ">";
	char *inout = isIn ? "in" : "out";
	int perm = isIn ? (O_RDONLY) : (O_RDWR | O_CREAT | O_TRUNC | S_IROTH);
	int stdinout = isIn ? (STDIN_FILENO) : (STDOUT_FILENO);
	int filedes;

	for (int i = 0; cmdvector[i] != NULL; i++){
		if (!strcmp(cmdvector[i], truncate)){
			if ((filedes = open(cmdvector[i + 1], perm, 0644)) == -1){
				
				for(int q = 0; q <= i + 1; q++){
					printf("q %d: %s\n", q, cmdvector[q]);
				}

				fatal("redirection");
			}
			dup2(filedes, stdinout);
			close(filedes);

			for (int j = i; cmdvector[j] != NULL; j++) cmdvector[j] = cmdvector[j + 2];
			
			break;
		}
	}
}

int slicingBetweenPipe(int i){
	int pCount = 0, cCount = 0, bcCount = 0;
	char *curCommand;

	for(int cCount = 0; cmdvector[cCount] != NULL; cCount++){
		curCommand = cmdvector[cCount];

		if(!strcmp(curCommand, "|")){
			pCount++;
			continue;
		}

		if(pCount < i) continue;
		if(i < pCount) break;

		betweenPipe[bcCount] = curCommand;
		bcCount++;
	}

	return bcCount;
}

void pipeCommands(int pipecount, int isoutRedirec){
	pid_t pid2;
	int _pipe[pipecount + 1][2];
	for (int i = 0; i < pipecount + 1; i++){
		memset(betweenPipe, '\0', MAX_CMD_ARG * sizeof(char));
		int betweenCount = slicingBetweenPipe(i);

		betweenPipe[betweenCount] = '\0';
	
		pipe(_pipe[i]); //pipe open
		pid2 = fork();

		if(pid2 == 0){
			if(i == 0){ //first
				close(_pipe[i][0]); //read close
				dup2(_pipe[i][1], STDOUT_FILENO); //write
				close(_pipe[i][1]); // write close
			}
			else if(i == pipecount){ //last, outRedirection
				dup2(_pipe[i- 1][0], STDIN_FILENO); //pipe read
				close(_pipe[i - 1][0]); //read close
				close(_pipe[i][1]);
				close(_pipe[i][0]);
				if(isoutRedirec){
					_redirection(0); //redirection
					betweenPipe[betweenCount - 1] = NULL;
					betweenPipe[betweenCount - 2] = NULL;
				}
			}
			else{ //etc
				dup2(_pipe[i - 1][0], STDIN_FILENO); //pipe read
				close(_pipe[i - 1][0]);

				close(_pipe[i][0]);
				dup2(_pipe[i][1], STDOUT_FILENO); //pipe write
				close(_pipe[i][1]);
			}
		
			execvp(betweenPipe[0], betweenPipe); //command between | execute
			exit(0);
			
			fatal("pid2 == 0");
		}
		else if(pid2 == -1) fatal("pipe");
		else{
			close(_pipe[i][1]);

			wait(NULL);
			continue;
		}	
	}
}



int main(int argc, char**argv){
	int stat, cmdcount, isBack;
	pid_t pid;
	signalSetting();


	while (1) {
		cmdline[0] = '\0';
		fputs(prompt, stdout);
		fgets(cmdline, CMD_BUFSIZ, stdin);
		cmdline[strlen(cmdline) - 1] = '\0';

		cmdcount = makelist(cmdline, " \t", cmdvector, MAX_CMD_ARG);
		

		if (cmdcount == 0) continue; //Just enter
		isBack = !strcmp(cmdvector[cmdcount - 1], "&"); //Background check
		if (isBack) cmdvector[cmdcount - 1] = NULL; //delete &
				

		//Change directory
		if (!strcmp(cmdvector[0], "cd")){
			if (chdir(cmdvector[1])) perror("cd()"); //error
			continue;
		}
		//Exit
		else if (!strcmp(cmdvector[0], "exit")) exit(0);

		pid = fork();
		if (pid == 0){ //child일 경우 == 명령어 실행 주체
			if(!isBack) signalSettingClear(); //foreground process일 경우 시그널 핸들러 초기화
			
			
			int isinRedirec = 0, isoutRedirec = 0, pipecount = 0;
			for (int i = 0; cmdvector[i] != NULL; i++){
				isinRedirec |= !strcmp(cmdvector[i], "<");
				isoutRedirec |= !strcmp(cmdvector[i], ">");
				pipecount += !strcmp(cmdvector[i], "|");
			}

			//if redirection exist, redirect and delete redirection command
			if (isinRedirec == 1) _redirection(1);
			
			if (pipecount == 0){ //no pipe
				if (isoutRedirec) _redirection(0);
				execvp(cmdvector[0], cmdvector);
				fatal("no pipe");
			}
			else{ // yes pipe
				pipeCommands(pipecount, isoutRedirec);
				exit(0);
				//fatal("pipeCommands");
			}

			fatal("in main()");
		}
		else if (pid == -1) fatal("main()");
		else{
			if (!isBack) waitpid(pid, &stat, WUNTRACED);
		}
	}
	return 0;
}
