#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void run_right(char *job, char *argl[]){
  
  if(fork() == 0){
    // exec in the child process
    exec(job, argl);
    exit(0);
  }

  // nothing to do in the parent process
  return;
}


int main(int argc, char *argv[])
{
  char buffer[2048]; // buffer for read-in
  char *front = buffer, *end = buffer;
  char *argbuf[1028]; // store all arguments
  char **args = argbuf; // point to the first arg from stdin

  for(int i = 1; i < argc; i++){ // arguments begin from the index 1
    *args = argv[i];
    args++;
  }

  char **pa = args; // pa is the start of current read-in
  while(read(0, front, 1) != 0){
    if(*front == ' ' || *front == '\n'){
      *front = '\0'; // set the division to '\0' for using string
      *(pa++) = end; // pa : colum ++; row not change
      end = front + 1;

      if(*front == '\n'){
        // current line is over, run the right and reset the pa
        *pa = 0;
        run_right(argv[1], argbuf);
        pa = args;
      }
    }
    front++;
  }

  // if the last line doesn't end with \n
  if(pa != args) { 
		*front = '\0';
		*(pa++) = end;
		*pa = 0;

		// exec the last command
		run_right(argv[1], argbuf);
	}

  while(wait(0) != -1) {}; // if all child have exited, wait() return -1
  exit(0);
}