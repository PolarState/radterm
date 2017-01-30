#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <getopt.h>
#include <sys/time.h>

#include <unistd.h> 
#include <termios.h>

#include "serial/serial.h"

// Struct for the driver state
typedef struct driver_t
{
  const char *pidfile;
  const char *logfile;
  const char *port;
  int   baud;
  int   fd;
  int   daemon;
  int   verbose;
  int   time;

} driver_t; 


static driver_t driver;

// enables and disables canonical mode for terminal input.
void ttyCanonicalMode(int state)
{
  struct termios ttystate;

  tcgetattr(STDIN_FILENO, &ttystate);

  switch(state)
  {
    case 1:
      ttystate.c_lflag &= ~ICANON;
      ttystate.c_cc[VMIN] = 1;
      break;

    default:
      ttystate.c_lflag |= ICANON;
      break;
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

static void exithandler()
{ 
  printf("Closing the connection.\n"); 
  serialClose(driver.fd); 
  ttyCanonicalMode(0);
}

static void error(char *msg) { fprintf(stderr, "Error: %s\n", msg); exit(1); }

static void take_ownership(const char *port) {
  int user  = getuid();
  int group = getgid();

  #ifdef DEBUG
  printf("\nUID:\t%d\nGID:\t%d\n\n", user, group);
  #endif

  // int chown(const char *path, uid_t owner, gid_t group);
  //@@ check error returned from chown
  if (chown(port, user, group) == -1) { 
    printf("Error (can't `chown`): %s\n Please run as admin\n", strerror(errno)); 
    exit(EXIT_FAILURE); 
  }
}


// loop checks for input on terminal and serial port & passes data to parsers.
void inputLoop(int * fdList, int fdListLength) 
{
  char            c;
  unsigned char   b;
  struct timeval  t;
  unsigned long long millisecondsSinceEpoch;
  int fdListMax;
  int result, readFault;
  fd_set fds;

  if(fdListLength == 0)
  {
    printf("no list length.\n");
    return;
  }


  c         = 0;
  fdListMax = 0;

  // find largest file descriptor
  for(int i = 0; i < fdListLength; i++)
  {
    fdListMax = (fdList[i] > fdListMax) ? fdList[i] : fdListMax;
  }

  // remove terminal canonical mode for parsing ease.
  ttyCanonicalMode(1);

  result    = 0;
  readFault = 0;
  while((result != -1) && readFault != 1)
  {
    FD_ZERO(&fds);
    for(int i = 0; i < fdListLength; i++)
      FD_SET(fdList[i], &fds);
    
    result = select(fdListMax+1, &fds, NULL, NULL, NULL);

    if(FD_ISSET(fdList[0], &fds))
    {
      if(read(fdList[0], &b, 1) == 1)
      {
        printf("%c", b);
        if(driver.time == 1 && b == '\n')
        {
          gettimeofday(&t, NULL);
          millisecondsSinceEpoch =
            (unsigned long long)(t.tv_sec)  * 1000 +
            (unsigned long long)(t.tv_usec) / 1000;
          printf("%11llu: ", millisecondsSinceEpoch);
        }
      }
      else
      {
        readFault = 1;
      }
    }
    else if(FD_ISSET(fdList[1], &fds))
    {
      c = fgetc(stdin);
      if(c != '\r')
        write(fdList[0], &c, 1);
    }
  }

  printf("\nexit.\n");
}

/*
 * starts the driver
 */ 
static void start(driver_t *driver) {

  // take_ownership(driver->port);

  driver->fd = serialOpen(driver->port, driver->baud);
    
  if (driver->fd == -1) {
    printf("Could not open %s with %d!\n", driver->port, driver->baud);
    exit(1);
  }

  atexit(exithandler);

  printf("Connected! Data received will be printed...\n");
  printf("Use CTRL+C to quit.\n");

  int fdList[2];
  fdList[0] = driver->fd;
  fdList[1] = STDIN_FILENO;

  //@@ change input loop input.
  inputLoop(fdList, 2);
}

//@@ add ability to run as background thread.
void daemonize(void)
{
  ;
}

// Prints correct usage.
int usage(void)
{
  fprintf(stderr, "-i --pidfile <path>  specify the pidfile location\n");
  fprintf(stderr, "-g --logfile <path>  specify the logfile location\n");
  fprintf(stderr, "-b --baud <num>      specify the baud rate, defaults to 9600\n");
  fprintf(stderr, "-p --port <path>     specify the port. If empty, tries to guess based on your platform\n");
  fprintf(stderr, "-v --verbose         enable verbose output\n");
  fprintf(stderr, "-d --daemon          run as a daemon in the background\n");
  return 0;
}

// entrypoint
int main(int argc, char **argv) {
  int c = 0, d = 0;

  // default states.
  driver.pidfile  = ""; //FIXME
  driver.logfile  = ""; //FIXME
  driver.port     = "/dev/USB0";
  driver.baud     = 9600;
  driver.fd       = -1;
  driver.daemon   = 0;
  driver.verbose  = 0;
  driver.time     = 0;

  printf("RaDtErM v0.1.0\n");

  // parse command line arguments.
  while((c = getopt(argc, argv, "i:g:b:p:vdt")) != EOF)
  {
    switch(c)
    {
      case 'b':
        driver.baud = atoi(optarg);
        break;
      case 'p':
        driver.port = optarg;
        break;
      case 'i':
        driver.pidfile = optarg;
        break;
      case 'g':
        driver.logfile = optarg;
        break;
      case 'd':
        d = 1;
        break;
      case 'v':
        driver.verbose = 1;
        break;
      case 't':
        driver.time = 1;
        break;
      default:
        fprintf(stderr, "unknown arg %c\n", optopt);
        return usage();
        break;
    }
  }

  printf("port = %s\n", driver.port);
  printf("baud rate = %i\n", driver.baud);

  if (d == 1)
    daemonize();

  start(&driver);

  ttyCanonicalMode(0);
  return 0;
}