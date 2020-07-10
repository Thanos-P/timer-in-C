/**
 * @file   timer.h
 * @author athanasps <athanasps@ece.auth.gr>
 *         Thanos Paraskevas
 *
 */

#ifndef TIMER_H
#define TIMER_H

#include "queue.h"

// Number of consumers
#define CONSUMERS_NUM 1

typedef struct {
  // Specification
  unsigned int Period;
  int TasksToExecute;
  unsigned int StartDelay;
  void * (*StartFcn)(void *);
  void * (*StopFcn)(void *);
  void * (*TimerFcn)(void *);
  void * (*ErrorFcn)(void *);
  void *Userdata;
  // Further timer parameters
  pthread_t pro;
  queue *fifo;
  long WaitTime;
} timer;

// Common global queue
queue *globalQueue;

// Count of active timers
volatile int active_timers = 0;
pthread_mutex_t timer_mut;

// Flag that indicates termination of consumers
volatile int consumerTerminationFlag = 0;
// Condition that indicated when a consumer is terminated
pthread_cond_t consumerTerminated;

// Producer and consumer function declaration
void *producer(void *args);
void *consumer(void *args);

// Consumer thread declaration
pthread_t con[CONSUMERS_NUM];

// Function to start a timer
void start(timer *t){
  // Start producer thread
  pthread_create(&(t->pro), NULL, producer, t);
}

// Function to wait until it is time to start producer
// (used in startat function bellow)
void *waitingProducer(void *args){
  timer *t = (timer *)args;

  // Sleep for the desired duration
  sleep(t->WaitTime);
  // Call producer function
  producer(args);

  return NULL;
}

// Function to start a timer at a desired date and time
void startat(timer *t, int year, int month, int day, int hour, int min, int sec){
  // Get current date and time
  time_t currtime = time(NULL);
  struct tm dt = *localtime(&currtime);

  // Set waiting time in seconds
  t->WaitTime = (year - dt.tm_year - 1900) * 31536000 +
                (month - dt.tm_mon - 1) * 2592000 +
                (day - dt.tm_mday) * 86400 +
                (hour - dt.tm_hour) * 3600 +
                (min - dt.tm_min) * 60 +
                (sec - dt.tm_sec);

  if(t->WaitTime < 0){
    printf("Invalid start time\n");
    return;
  }

  printf("Waiting for %ld seconds\n", t->WaitTime);
  // Create a thread to wait until it is time to start the producer
  pthread_create(&(t->pro), NULL, waitingProducer, t);
}

// Timer initialization function
int timerInit(timer *t, unsigned int Period, int TasksToExecute, unsigned int StartDelay,
              void * (*StartFcn)(void *), void * (*StopFcn)(void *),
              void * (*TimerFcn)(void *), void * (*ErrorFcn)(void *),
              void *Userdata){

  // Initialize timer parameters
  t->Period = Period;
  t->TasksToExecute = TasksToExecute;
  t->StartDelay = StartDelay;
  t->StartFcn = StartFcn;
  t->StopFcn = StopFcn;
  t->TimerFcn = TimerFcn;
  t->ErrorFcn = ErrorFcn;
  t->Userdata = Userdata;

  // critical section (to keep active_timers thread-safe)
  pthread_mutex_lock(&timer_mut);

  // If this is the first active timer
  if(active_timers == 0){
    // Initialize timer queue
    globalQueue = queueInit();
    if(globalQueue ==  NULL){
      pthread_mutex_unlock(&timer_mut);
      return -1;
    }

    // Initialize consumer termination condition
    pthread_cond_init(&consumerTerminated, NULL);

    // Start consumer threads
    for(int i = 0; i < CONSUMERS_NUM; i++){
      pthread_create(&con[i], NULL, consumer, globalQueue);
    }
  }
  // increment active timer count
  active_timers++;

  pthread_mutex_unlock(&timer_mut);

  // Link timer to global queue
  t->fifo = globalQueue;

  return 0;
}

// Function that waits for timer execution
void timerWait(void){
  // Join consumer threads
  for(int i = 0; i < CONSUMERS_NUM; i++){
    pthread_join (con[i], NULL);
  }
}


// Producer and consumer function implementation

void *producer(void *args){
  timer *t = (timer *)args;

  workFunction input;
  // Set lastItemFlag to false
  input.lastItemFlag = 0;

  // ====================== Call StartFcn ======================
  // Set function
  input.work = t->StartFcn;
  // Set function arguement
  input.arg = t->Userdata;

  // Add element to queue
  pthread_mutex_lock(t->fifo->mut);
  while(t->fifo->full){
    printf("producer: queue FULL.\n");
    // Call error function
    if(t->ErrorFcn != NULL){
      t->ErrorFcn(t->Userdata);
    }
    pthread_cond_wait(t->fifo->notFull, t->fifo->mut);
  }
  // Start timer for the item
  gettimeofday(&(input.startwtime), NULL);

  queueAdd(t->fifo, input);
  pthread_mutex_unlock(t->fifo->mut);
  pthread_cond_signal(t->fifo->notEmpty);

  // Wait for first call of TimerFcn
  sleep(t->StartDelay);

  // ================ Repeatedly call TimerFcn ================
  for (int i = t->TasksToExecute; i != 0; i--) {
    // Set function
    input.work = t->TimerFcn;
    // Set function arguement
    input.arg = (void *)i;

    // Add element to queue
    pthread_mutex_lock(t->fifo->mut);
    while(t->fifo->full){
      printf("producer: queue FULL.\n");
      // Call error function
      if(t->ErrorFcn != NULL){
        t->ErrorFcn(t->Userdata);
      }
      pthread_cond_wait(t->fifo->notFull, t->fifo->mut);
    }
    // Start timer for the item
    gettimeofday(&(input.startwtime), NULL);

    queueAdd(t->fifo, input);
    pthread_mutex_unlock(t->fifo->mut);
    pthread_cond_signal(t->fifo->notEmpty);

    // if this is not the last iteration
    if(i != 1){
      // Wait for the next call
      usleep(1000 * t->Period);
    }

    // if TasksToExecute (therefore i) is negative
    if(i < 0){
      // Set i to 0 on every iteration
      // so it always starts the loop with value -1
      i = 0;
    }
  }

  // ====================== Call StopFcn ======================
  // Set function
  input.work = t->StopFcn;
  // Set function arguement
  input.arg = t->Userdata;
  // Set lastItemFlag to true on last item added for the timer
  input.lastItemFlag = 1;

  // Add element to queue
  pthread_mutex_lock(t->fifo->mut);
  while(t->fifo->full){
    printf("producer: queue FULL.\n");
    // Call error function
    if(t->ErrorFcn != NULL){
      t->ErrorFcn(t->Userdata);
    }
    pthread_cond_wait(t->fifo->notFull, t->fifo->mut);
  }
  // Start timer for the item
  gettimeofday(&(input.startwtime), NULL);

  queueAdd(t->fifo, input);
  pthread_mutex_unlock(t->fifo->mut);
  pthread_cond_signal(t->fifo->notEmpty);

  return NULL;
}

void *consumer(void *args){
  queue *fifo = (queue *)args;

  workFunction output;

  while(1){
    // Get element from queue
    pthread_mutex_lock(fifo->mut);
    while(fifo->empty){
      printf("consumer: queue EMPTY.\n");

      // Check consumerTerminationFlag
      if(consumerTerminationFlag){
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(&consumerTerminated);
        // Terminate consumer
        return NULL;
      }
      pthread_cond_wait(fifo->notEmpty, fifo->mut);
    }
    queueDel(fifo, &output);

    // Stop timer for the item
    gettimeofday(&(output.endwtime), NULL);
    // fprintf(fp, "%f\n", (double)((output.endwtime.tv_usec - output.startwtime.tv_usec)/1.0e6
    //         + output.endwtime.tv_sec - output.startwtime.tv_sec));

    pthread_mutex_unlock(fifo->mut);
    pthread_cond_signal(fifo->notFull);

    // Call function with arguement
    if(output.work != NULL){
      output.work(output.arg);
    }

    // If this item is flagged as the last
    if(output.lastItemFlag){
      // critical section (to keep active_timers thread-safe)
      pthread_mutex_lock(&timer_mut);

      // decrement active timer count
      active_timers--;

      // If this is the last active timer
      if(active_timers == 0){
        // Set consumerTerminationFlag to true
        consumerTerminationFlag = 1;

        // Terminate all other consumer threads
        for(int i = 0; i < CONSUMERS_NUM-1; i++){
          // Notify one consumer to detect the termination flag
          pthread_cond_signal(fifo->notEmpty);
          // Wait until current consumer terminates to move to the next
          pthread_cond_wait(&consumerTerminated, &timer_mut);
        }

        pthread_mutex_unlock(&timer_mut);

        // Destroy timer queue and condition
        pthread_cond_destroy(&consumerTerminated);
        queueDelete(fifo);

        // Terminate current consumer
        return NULL;
      }

      pthread_mutex_unlock(&timer_mut);
    }
  }

  return NULL;
}

#endif /* TIMER_H */
