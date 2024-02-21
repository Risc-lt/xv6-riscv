#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
  First version of primes
  disadvantage: too many fork in the main
*/

// int main(int argc, char *argv[])
// {
//   int p1[2], p2[2], p3[2], p4[2], p5[2];

//   pipe(p1);
//   pipe(p2);
//   pipe(p3); 
//   pipe(p4);
//   pipe(p5);
  
//   if(fork() != 0){
//     // feed numbers to p1
//     int buff[31];
//     buff[0] = 2;
//     for(int i = 1; i < 31; i++)
//       buff[i] = buff[i-1] + 1;
//     // write to p1
//     write(p1[1], buff, sizeof(buff));
//     close(p1[1]);

//     wait(EXIT_SUCESS);
//     exit(0);
//   } else{
//     // the first primes sieve
//     int buff[31];

//     // read from p1
//     read(p1[0], buff, sizeof(buff));
//     printf("primes %d", buff[0]);
//     close(p1[0]);

//     fliter(buff);

//     if(fork() != 0){
//       // write to p2
//       write(p2[1], buff, sizeof(buff));
//       close(p2[1]);
//     }
    

//     }

// }

void sieve(int pl[2]){
  // print the number in the front
  int out;
  read(pl[0], &out, sizeof(out));

  // exit recursive if no num left
  if(out == 0){
    exit(0);
  }
  
  printf("primes %d\n", out);
  
  int pr[2];
  pipe(pr);

  if(fork() == 0){
    // close the pipe and switch to the child
    // printf("successful fork\n"); // for testing
    close(pl[0]);
    close(pr[1]);
    sieve(pr);

  } else{
    // printf("parent init\n"); // for testing

    // close the read port after the child
    close(pr[0]);

    // fliter the number
    int buff;
    while(read(pl[0], &buff, sizeof(buff)) && buff != 0){
      // printf("%d\n", buff); // for testimg
      if(buff % out != 0){
        write(pr[1], &buff, sizeof(buff));
      }
    }

    // set the tail number to 0
    buff = 0;
    write(pr[1], &buff, sizeof(buff));

    // printf("parent done\n"); // for testing
  
    wait(0);
    exit(0);

  }

}

int main(int argc, char *argv[])
{
  int init_pipe[2];

  pipe(init_pipe);
  
  if(fork() == 0){
    // close the pipe and switch to the child
    // printf("successful fork\n"); // for testing
    close(init_pipe[1]);
    sieve(init_pipe);
    exit(0);

  } else{
    // printf("parent init\n"); // for testing

    // close the read port
    close(init_pipe[0]);

    // initialize the number
    int buff = 2;
    while(buff < 33){
      write(init_pipe[1], &buff, sizeof(buff));
      buff++;
    }

    // set the tail number to 0
    buff = 0;
    write(init_pipe[1], &buff, sizeof(buff));

    // printf("parent done\n"); // for testing
  
    wait(0);
    exit(0);
  }
}