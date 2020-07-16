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


int main (){
  pthread_mutex_init(&timer_mut, NULL);

	// ~~~~~~~~ Initialize timer #1 ~~~~~~~~
  timer t1;
  unsigned int Period = 1000;
  unsigned int TasksToExecute = 3600;
  unsigned int StartDelay = 0;
  void *Userdata = (void *)malloc(sizeof(int));
	*((int *)Userdata) = 1000;

  if(timerInit(&t1, Period, TasksToExecute, StartDelay, myStartFun, myStopFun,
               myTimerFun, myErrorFun, Userdata) == -1){

    fprintf(stderr, "main: Timer Init failed.\n");
    exit(1);
  }

  // ~~~~~~~~~~ Start timer #1 ~~~~~~~~~~
  start(&t1);


	// ~~~~~~~~ Initialize timer #2 ~~~~~~~~
	timer t2;
	Period = 100;
  TasksToExecute = 36000;
  StartDelay = 0;

	if(timerInit(&t2, Period, TasksToExecute, StartDelay, myStartFun, myStopFun,
               myTimerFun, myErrorFun, Userdata) == -1){

    fprintf(stderr, "main: Timer Init failed.\n");
    exit(1);
  }

	// ~~~~~~~~~~ Start timer #2 ~~~~~~~~~~
	start(&t2);


	// ~~~~~~~~ Initialize timer #3 ~~~~~~~~
	timer t3;
	Period = 10;
  TasksToExecute = 360000;
  StartDelay = 0;

	if(timerInit(&t3, Period, TasksToExecute, StartDelay, myStartFun, myStopFun,
               myTimerFun, myErrorFun, Userdata) == -1){

    fprintf(stderr, "main: Timer Init failed.\n");
    exit(1);
  }

	// ~~~~~~~~~~ Start timer #3 ~~~~~~~~~~
	start(&t3);


	// Wait for timer execution
	timerWait();

	free(Userdata);

	pthread_mutex_destroy(&timer_mut);

  return 0;
}
