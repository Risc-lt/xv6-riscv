#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
  FIrst version of Ping-pong.
  fault: 1. use the same pipeline for two directions of reading and writing. 
         2. donnot initialize the buffer;
*/

// int main(int argc, char *argv[])
// {
//   int p[2];
//   char buff[2];

//   pipe(p);

//   if(fork() == 0){
    
//     read(p[0], buff, 1);
//     close(p[0]);
//     printf("%d: received ping", getpid());
//     write(p[1], buff, 1);
//     close(p[1]);

//     exit(0);

//   } else{
//     wait(0);

//     read(p[1], buff, 1);
//     printf("%d: received pong", getpid());

//     close(p[0]);
//     close(p[1]);

//     exit(0);
//   }
// }

int main(int argc, char *argv[])
{
  int parent2child[2], child2parent[2];

  pipe(parent2child);
  pipe(child2parent);

  if(fork() != 0){
    // send 1 byte to child
    char *buff = "9";
    write(parent2child[1], buff, 1);
    close(parent2child[1]);

    // receive 1 byte from child
    read(child2parent[0], buff, 1);
    printf("%d: received pong\n", getpid());
    close(child2parent[0]);
    
    exit(0);

  } else{
    // wait for child to exit
    wait(0);

    // receive 1 byte from parent
    char buff[2];
    read(parent2child[0], buff, 1);
    printf("%d: received ping\n", getpid());
    close(parent2child[0]);

    // send 1 byte to parent
    write(child2parent[1], buff, 1);
    close(child2parent[1]);

    exit(0);
  }
}