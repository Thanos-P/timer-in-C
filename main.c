/**
 * @file   main.c
 * @author athanasps <athanasps@ece.auth.gr>
 *         Thanos Paraskevas
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "timer.h"

// ================ Timer functions implementation ================
void *myTimerFun(void *arg)
{
	int n = (int)arg;
	if(n == -1)
		printf("my function: remaining calls = inf\n");
	else
		printf("my function: remaining calls = %d\n", n-1);

	return NULL;
}

void *myStartFun(void *arg)
{
	printf("This is the START function with Userdata=%d\n", *((int *)arg));
	return NULL;
}

void *myStopFun(void *arg)
{
	printf("This is the STOP function with Userdata=%d\n", *((int *)arg));
	return NULL;
}

void *myErrorFun(void *arg)
{
	printf("This is the ERROR function with Userdata=%d\n", *((int *)arg));
	return NULL;
}
// ================================================================

// Results file pointer
// FILE *fp;

// Function for closing the file pointer on termination
// void termination(int signum){
//   printf("\nTerminating...\n");
//   fclose(fp);
//   exit(0);
// }

int main (){
  // Initialize file pointer
  // char filename[30];
  // sprintf(filename, "results_n=%d_m=%d.csv", PRODUCERS_NUM, CONSUMERS_NUM);
  // fp = fopen(filename, "a");

  // Redirect control to termination function on <CTRL>+C
  // signal(SIGINT, termination);

  pthread_mutex_init(&timer_mut, NULL);

	// Initialize timer
  timer t1;
  unsigned int Period = 1000;
  int TasksToExecute = 5;
  unsigned int StartDelay = 0;
  void *Userdata = (void *)malloc(sizeof(int));
	*((int *)Userdata) = 1000;

  if(timerInit(&t1, Period, TasksToExecute, StartDelay, myStartFun, myStopFun,
               myTimerFun, myErrorFun, Userdata) == -1){

    fprintf(stderr, "main: Timer Init failed.\n");
    exit(1);
  }

  // Start timer
  start(&t1);


	// Initialize timer
	timer t2;
	Period = 500;
  TasksToExecute = 5;
  StartDelay = 0;

	if(timerInit(&t2, Period, TasksToExecute, StartDelay, myStartFun, myStopFun,
               myTimerFun, myErrorFun, Userdata) == -1){

    fprintf(stderr, "main: Timer Init failed.\n");
    exit(1);
  }

	// Start timer
	start(&t2);

	// Wait for timer execution
	timerWait();

	free(Userdata);

	pthread_mutex_destroy(&timer_mut);

  // fclose(fp);

  return 0;
}
