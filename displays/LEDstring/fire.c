
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>	// Defines signal-handling functions (i.e. trap Ctrl-C)

#include "fire.h"
#include "gpio.h"

/****************************************************************
 * Constants
 ****************************************************************/
 
#define POLL_TIMEOUT (60 * 1000) /* 60 seconds */
#define MAX 50		// Brightness

/****************************************************************
 * Global variables
 ****************************************************************/
int string_len = 160;
static FILE *rgb_fp;
int keepgoing = 1;	// Set to 0 when ctrl-c is pressed

/* Global thread environment */
int fire_env = 0;

/* Thread handle */
pthread_t fireThread;

/****************************************************************
 * signal_handler
 ****************************************************************/
// Callback called when SIGINT is sent to the process (Ctrl-C)
void signal_handler(int sig)
{
	printf( "Ctrl-C pressed, cleaning up and exiting..\n" );
	keepgoing = 0;
}

void display() {
	  fprintf(rgb_fp, "0 0 0 -1\n");
}

void rgb(int red, int green, int blue, int index, int us) {
	fprintf(rgb_fp, "%d %d %d %d", red, green, blue, index);
	if(us) {
	        display();
//    		printf("sending %d %d %d %d for %d\n", red, green, blue, index, us);
        }
        usleep(us);
}

void up() {
	int i;
	for(i=0; i<string_len; i++) {
//		printf("Fire: %d, %d!\n", count, i);
		rgb( 0,  0,  0, i-1,   0);
		rgb(MAX, MAX, MAX,   i, 50000);
	}
 }

void down() {
	int i;
	for(i=string_len-1; i>=-1; i--) {
//		printf("Fire: %d, %d!\n", count, i);
		rgb( 0,  0,  0, i  ,   0);
		rgb( 0, MAX,  0, i-1, 20000);
	}
}

void *fire(void *env) {
	int *tmp = env;
	int dir = *tmp;	// Initial direction
	switch(dir) {
	    case 0:
		down();
		up();
		break;
	    case 1:
		up();
		down();
	    break;
	}
	pthread_exit(NULL);
}

/****************************************************************
 * Main
 ****************************************************************/
int main(int argc, char **argv, char **envp)
{
	struct pollfd fdset[2];
	int nfds = 2;
	int gpio_fd, timeout, rc;
	char buf[MAX_BUF];
	unsigned int gpio;
	int len;
	char lastValue='0';
	int dir=1;	// Passed to fire to give inital direction

	if (argc < 2) {
		printf("Usage: fire <gpio-pin>\n\n");
		printf("Waits for a change in the GPIO pin voltage level or input on stdin\n");
		exit(-1);
	}

	// Set the signal callback for Ctrl-C
	signal(SIGINT, signal_handler);

	rgb_fp = fopen("/sys/firmware/lpd8806/device/rgb", "w");
	setbuf(rgb_fp, NULL);	// Turn buffering off
	if(rgb_fp == NULL) {
		printf("Opening rgb failed\n");
		exit(0);
	}

	gpio = atoi(argv[1]);
	if(argc == 3) {
		dir = atoi(argv[2]);
	}
    string_len = atoi(getenv("STRING_LEN"));
    if(string_len == 0) {
      string_len = 160;
    }
  printf("string_len = %d\n", string_len);
  


	gpio_export(gpio);
	gpio_set_dir(gpio, 0);
	gpio_set_edge(gpio, "rising");  // Can be rising, falling or both
	gpio_fd = gpio_fd_open(gpio);

	timeout = POLL_TIMEOUT;
 
	while (keepgoing) {
		memset((void*)fdset, 0, sizeof(fdset));

		fdset[0].fd = gpio_fd;
		fdset[0].events = POLLPRI;

		fdset[1].fd = STDIN_FILENO;
		fdset[1].events = POLLIN;
      
		rc = poll(fdset, 1, timeout);      

		if (rc < 0) {
			printf("\npoll() failed!\n");
			return -1;
		}
      
		if (rc == 0) {
			printf(".");
		}
            
		if (fdset[0].revents & POLLPRI) {
			lseek(fdset[0].fd, 0, SEEK_SET);  // Read from the start of the file
			len = read(fdset[0].fd, buf, MAX_BUF);
			printf("\npoll() GPIO %d interrupt occurred, value=%c, len=%d\n",
				 gpio, buf[0], len);
//			if(buf[0] != lastValue) {
				fire_env = dir;
				pthread_create(&fireThread, NULL,
						&fire, &fire_env);
//				lastValue = buf[0];
//			}
		}

		if (fdset[1].revents & POLLIN) {
			(void)read(fdset[1].fd, buf, 1);
			printf("\npoll() stdin read 0x%2.2X\n", (unsigned int) buf[0]);
		}

		fflush(stdout);
	}

	gpio_fd_close(gpio_fd);
	return 0;
}

