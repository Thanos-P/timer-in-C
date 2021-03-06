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
  // Timer object specification
  unsigned int Period;
  unsigned int TasksToExecute;
  unsigned int StartDelay;
  void * (*StartFcn)(void *);
  void * (*StopFcn)(void *);
  void * (*TimerFcn)(void *);
  void * (*ErrorFcn)(void *);
  void *Userdata;
  // Auxiliary timer parameters
  pthread_t pro;
  queue *fifo;
  long WaitTime;
} timer;

// *** GLOBAL VARIABLES ***

// Common global queue
queue *globalQueue;

// Count of active timers
volatile int active_timers = 0;
pthread_mutex_t timer_mut;

// Flag that indicates termination of consumers
volatile int consumerTerminationFlag = 0;
// Condition that indicates when a consumer is terminated
pthread_cond_t consumerTerminated;

// Consumer thread declaration
pthread_t con[CONSUMERS_NUM];

// Global consumer results file pointer
FILE *cons_fp;
// Mutex for writing to the file
pthread_mutex_t file_mut;

// *** FUNCTIONS ***

// Producer and consumer function declaration
void *producer(void *args);
void *consumer(void *args);

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

// Function to start a timer
void start(timer *t){
  // Start producer thread
  pthread_create(&(t->pro), NULL, producer, t);
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
    printf("Invalid start time, starting timer now\n");
    // Set waiting time to 0 in case of a past start time
    t->WaitTime = 0;
  }else{
    printf("Waiting for %ld seconds\n", t->WaitTime);
  }

  // Create a thread to wait until it is time to start the producer
  pthread_create(&(t->pro), NULL, waitingProducer, t);
}

// Timer initialization function
int timerInit(timer *t, unsigned int Period, unsigned int TasksToExecute,
              unsigned int StartDelay,
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

    // Initialize consumer results file pointer and mutex
    cons_fp = fopen("consumer_times.csv", "a");
    pthread_mutex_init(&file_mut, NULL);
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
    pthread_join(con[i], NULL);
  }
}


// Producer and consumer function implementation

void *producer(void *args){
  timer *t = (timer *)args;

  // Initialize timeval structures for measuring time drift
  struct timeval *currentStartTime, *previousStartTime, *temp;
  currentStartTime = (struct timeval *)malloc(sizeof(struct timeval));
  previousStartTime = (struct timeval *)malloc(sizeof(struct timeval));
  int timeDrift = 0;
  // Initialize adjusted Period (it will be tuned according to timeDrift below)
  unsigned int adjustedPeriod = 1000 * t->Period;

  // Drift time results file pointer (separate for each producer)
  FILE *fp;
  // Initialize file pointer
  char filename[20];
  sprintf(filename, "drifting_T=%u.csv", t->Period);
  fp = fopen(filename, "a");

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
    // Call error function
    if(t->ErrorFcn != NULL){
      t->ErrorFcn(t->Userdata);
    }
    pthread_cond_wait(t->fifo->notFull, t->fifo->mut);
  }
  // Start queue timer for the item
  gettimeofday(&(input.queueStartTime), NULL);

  queueAdd(t->fifo, input);
  pthread_mutex_unlock(t->fifo->mut);
  pthread_cond_signal(t->fifo->notEmpty);

  // Wait for first call of TimerFcn
  sleep(t->StartDelay);

  // ================ Repeatedly call TimerFcn ================
  for (unsigned int i = t->TasksToExecute; i > 0; i--) {
    // Measure current start time of producer execution
    gettimeofday(currentStartTime, NULL);
    // If this is not the first iteration
    if(i != t->TasksToExecute){
      // Calculate time drift in microseconds
      timeDrift = (currentStartTime->tv_sec - previousStartTime->tv_sec) * 1.0e6
                  + (currentStartTime->tv_usec - previousStartTime->tv_usec)
                  - t->Period * 1.0e3;
    }
    // Swap times
    temp = previousStartTime;
    previousStartTime = currentStartTime;
    currentStartTime = temp;
    // Print result to file
    fprintf(fp, "%d\n", timeDrift);

    // Set function
    input.work = t->TimerFcn;
    // Set function arguement
    input.arg = t->Userdata;

    // Add element to queue
    pthread_mutex_lock(t->fifo->mut);
    if(t->fifo->full){
      // Call error function
      if(t->ErrorFcn != NULL){
        t->ErrorFcn(t->Userdata);
      }
      // drop current function call, in case of a full queue
      pthread_mutex_unlock(t->fifo->mut);
      // Wait for the next call
      usleep(adjustedPeriod);
      continue;
    }
    // Start queue timer for the item
    gettimeofday(&(input.queueStartTime), NULL);

    queueAdd(t->fifo, input);
    pthread_mutex_unlock(t->fifo->mut);
    pthread_cond_signal(t->fifo->notEmpty);

    // if this is not the last iteration
    if(i > 1){
      // Adjust the Period
      if((long)adjustedPeriod - timeDrift > 0){
        adjustedPeriod -= timeDrift;
      }else{
        adjustedPeriod = 0;
      }
      // Wait for the next call
      usleep(adjustedPeriod);
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
    // Call error function
    if(t->ErrorFcn != NULL){
      t->ErrorFcn(t->Userdata);
    }
    pthread_cond_wait(t->fifo->notFull, t->fifo->mut);
  }
  // Start queue timer for the item
  gettimeofday(&(input.queueStartTime), NULL);

  queueAdd(t->fifo, input);
  pthread_mutex_unlock(t->fifo->mut);
  pthread_cond_signal(t->fifo->notEmpty);

  fclose(fp);
  free(currentStartTime);
  free(previousStartTime);

  return NULL;
}


void *consumer(void *args){
  queue *fifo = (queue *)args;

  workFunction output;

  // Initialize timeval structures for measuring extraction time by consumer
  struct timeval delStartTime, delEndTime;
  int delTime, queueTime;

  while(1){
    // Get element from queue
    pthread_mutex_lock(fifo->mut);
    while(fifo->empty){
      // printf("consumer: queue EMPTY.\n");
      // Check consumerTerminationFlag
      if(consumerTerminationFlag){
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(&consumerTerminated);
        // Terminate consumer
        return NULL;
      }
      pthread_cond_wait(fifo->notEmpty, fifo->mut);
    }

    // Start extraction timer
    gettimeofday(&delStartTime, NULL);

    queueDel(fifo, &output);

    pthread_mutex_unlock(fifo->mut);
    pthread_cond_signal(fifo->notFull);

    // Stop extraction timer
    gettimeofday(&delEndTime, NULL);

    // Compute times
    delTime = (delEndTime.tv_sec - delStartTime.tv_sec) * 1.0e6
              + (delEndTime.tv_usec - delStartTime.tv_usec);
    queueTime = (delEndTime.tv_sec - output.queueStartTime.tv_sec) * 1.0e6
                + delEndTime.tv_usec - output.queueStartTime.tv_usec
                - delTime;

    // Write results to file (thread-safe)
    pthread_mutex_lock(&file_mut);
    fprintf(cons_fp, "%d,", queueTime);
    fprintf(cons_fp, "%d\n", delTime);
    pthread_mutex_unlock(&file_mut);

    // Call function with arguement
    if(output.work != NULL){
      output.work(output.arg);
    }

    // ======= TERMINATION CONDITION =======
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
        // Close consumer global file and mutex
        fclose(cons_fp);
        pthread_mutex_destroy(&file_mut);

        // Terminate current consumer
        return NULL;
      }

      pthread_mutex_unlock(&timer_mut);
    }
  }

  return NULL;
}

#endif /* TIMER_H */
