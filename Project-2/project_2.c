#include "queue.c"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <limits.h>

int simulationTime = 120; // simulation time
int seed = 10; // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2; // probability of a ground job (launch & assembly)

void * LandingJob(void * arg);
void * LaunchJob(void * arg);
void * EmergencyJob(void * arg);
void * AssemblyJob(void * arg);
void * ControlTower(void * arg);

//Thread functions for Pad A and Pad B
void * PadA(void * arg);
void * PadB(void * arg);

//Shared global time
time_t startTime;
time_t endTime;
struct timeval current_time;

//t variable
int t = 2;

//n variable, if no value will be given then no prints will be seen
time_t n = INT_MAX;

//ID variable for the jobs first job's id is 1
int ID = 1;

//File pointer for rockets.log
FILE * fp;

//Mutexes for Pad A and Pad B
pthread_mutex_t mutexA;
pthread_mutex_t mutexB;

//Mutex for protecting ID
pthread_mutex_t mutex;

//Time mutex
pthread_mutex_t timeMutex;

//Mutex and cond for starting the tower
pthread_mutex_t start;
pthread_cond_t startCond;
pthread_mutex_t startA;
pthread_cond_t padAStart;
pthread_mutex_t startB;
pthread_cond_t padBStart;

//Mutexes for queues
pthread_mutex_t landingMutex;
pthread_mutex_t launchingMutex;
pthread_mutex_t assemblyMutex;
pthread_mutex_t emergencyMutex;
pthread_mutex_t padAQueueMutex;
pthread_mutex_t padBQueueMutex;

//Queues for different job types and pads
Queue * launchingQueue;
Queue * landingQueue;
Queue * assemblyQueue;
Queue * padAQueue;
Queue * padBQueue;
Queue * emergencyQueue;

// pthread sleeper function
int pthread_sleep(int seconds) {
  pthread_mutex_t mutex;
  pthread_cond_t conditionvar;
  struct timespec timetoexpire;
  if (pthread_mutex_init(&mutex, NULL)) {
    return -1;
  }
  if (pthread_cond_init(&conditionvar, NULL)) {
    return -1;
  }
  struct timeval tp;
  //When to expire is an absolute time, so get the current time and add it to our delay time
  gettimeofday( & tp, NULL);
  timetoexpire.tv_sec = tp.tv_sec + seconds;
  timetoexpire.tv_nsec = 0;
  tp.tv_usec * 1000;

  pthread_mutex_lock(&mutex);
  int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&conditionvar);

  //Upon successful completion, a value of zero shall be returned
  return res;
}

int main(int argc, char ** argv) {
  // -p (float) => sets p
  // -t (int) => simulation time in seconds
  // -s (int) => change the random seed
  // -n (timt_t(long int)) => set n
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-p")) {
      p = atof(argv[++i]);
    } else if (!strcmp(argv[i], "-t")) {
      simulationTime = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "-s")) {
      seed = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "-n")) {
      n = atoi(argv[++i]);
    }
  }

  srand(seed); // feed the seed

  //Initializing the queues
  launchingQueue = ConstructQueue(1000);
  landingQueue = ConstructQueue(1000);
  assemblyQueue = ConstructQueue(1000);
  emergencyQueue = ConstructQueue(1000);
  
  //Pads has one size
  padAQueue = ConstructQueue(1);
  padBQueue = ConstructQueue(1);
  
  //Initializing mutex and conds
  if (pthread_mutex_init(&mutexA, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&mutexB, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&mutex, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&start, NULL))
    printf("Mutex init failed.\n");
  if (pthread_cond_init(&startCond, NULL))
    printf("Cond init failed.\n");
  if (pthread_mutex_init(&landingMutex, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&launchingMutex, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&assemblyMutex, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&emergencyMutex, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&padAQueueMutex, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&padBQueueMutex, NULL))
    printf("Mutex init failed.\n");
  if (pthread_cond_init(&padAStart, NULL))
    printf("Cond init failed.\n");
  if (pthread_cond_init(&padBStart, NULL))
    printf("Cond init failed.\n");
  if (pthread_mutex_init(&startA, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&startB, NULL))
    printf("Mutex init failed.\n");
  if (pthread_mutex_init(&timeMutex, NULL))
    printf("Mutex init failed.\n");

  //Opens the rockets.log file and starts wiritng
  fp = fopen("rockets.log", "w");
  fprintf(fp, "%s", "JobID   Type  Request Time  End Time  Turnaround Time   Pad   Size\n");
  fprintf(fp, "%s", "___________________________________________________________________\n");

  //Threads for tower, Pad A, Pad B, and first land job
  pthread_t airControllerThread;
  pthread_t padAThread;
  pthread_t padBThread;
  pthread_t firstLand;

  //Setting time variables
  if (gettimeofday(&current_time, NULL) == -1)
    printf("Getting time of the day failed!.\n");
  startTime = current_time.tv_sec;
  endTime = current_time.tv_sec + simulationTime;

  //Creating threads for tower, Pad A, Pad B, and first land job
  pthread_create(&airControllerThread, NULL, ControlTower, NULL);
  pthread_create(&padAThread, NULL, PadA, NULL);
  pthread_create(&padBThread, NULL, PadB, NULL);

  //Creating the first land job and giving its requestTime
  time_t * currentSimulationTime = (time_t * ) malloc(sizeof(time_t));
  * currentSimulationTime = 0;
  pthread_create(&firstLand, NULL, LaunchJob, (void * ) currentSimulationTime);

  pthread_sleep(t);
  pthread_mutex_lock(&timeMutex);
  if (gettimeofday(&current_time, NULL) == -1)
    printf("Getting time of the day failed!.\n");
  pthread_mutex_unlock(&timeMutex);

  //Main loop for creating job threads according to probabilities
  while (current_time.tv_sec < endTime) {
    //Requst time for jobs
    time_t * currentSimulationTime = (time_t * ) malloc(sizeof(time_t));
    * currentSimulationTime = current_time.tv_sec - startTime;
    
    //Prob generating
    double random1 = (double) rand() / (RAND_MAX);
    double random2 = (double) rand() / (RAND_MAX);
    double random3 = (double) rand() / (RAND_MAX);

    //Creating jobs/threads
    if ((current_time.tv_sec - startTime) % (40 * t) == 0) {
      pthread_t jobThread1;
      pthread_t jobThread2;
      pthread_create(&jobThread1, NULL, EmergencyJob, (void * ) currentSimulationTime);
      pthread_create(&jobThread2, NULL, EmergencyJob, (void * ) currentSimulationTime);
    }
    if (random3 <= 1 - p) {
      pthread_t jobThread;
      pthread_create(&jobThread, NULL, LandingJob, (void * ) currentSimulationTime);
    }
    if (random1 <= p / 2) {

      pthread_t jobThread;
      pthread_create(&jobThread, NULL, AssemblyJob, (void * ) currentSimulationTime);
    }
    if (random2 <= p / 2) {

      pthread_t jobThread;
      pthread_create(&jobThread, NULL, LaunchJob, (void * ) currentSimulationTime);
    }
    
    //Printing queues
    if(n <= *currentSimulationTime ) {
         printf("At %ld sec landing : ", current_time.tv_sec - startTime);
         printQueueId(landingQueue);
         printf("At %ld sec launching : ", current_time.tv_sec - startTime);
         printQueueId(launchingQueue);
         printf("At %ld sec assembly : ", current_time.tv_sec - startTime);
         printQueueId(assemblyQueue);
         printf("\n");
    }

    pthread_sleep(t);
    pthread_mutex_lock(&timeMutex);
    if (gettimeofday( & current_time, NULL) == -1)
      printf("Getting time of the day failed!.\n");
    pthread_mutex_unlock(&timeMutex);
  }

  //Join the threads
  pthread_join(airControllerThread, NULL);
  pthread_join(padAThread, NULL);
  pthread_join(padBThread, NULL);
      
  //Destructing queues
  DestructQueue(launchingQueue);
  DestructQueue(landingQueue);
  DestructQueue(assemblyQueue);
  DestructQueue(emergencyQueue);
  DestructQueue(padAQueue);
  DestructQueue(padBQueue);

  //Destroying mutexes
  pthread_mutex_destroy(&mutexA);
  pthread_mutex_destroy(&mutexB);
  pthread_mutex_destroy(&mutex);
  pthread_mutex_destroy(&start);
  pthread_cond_destroy(&startCond);
  pthread_mutex_destroy(&landingMutex);
  pthread_mutex_destroy(&launchingMutex);
  pthread_mutex_destroy(&assemblyMutex);
  pthread_mutex_destroy(&padAQueueMutex);
  pthread_mutex_destroy(&padBQueueMutex);

  //Closing rockets.log     
  fclose(fp);
  free(currentSimulationTime);
  exit(0);
}

// the function that creates plane threads for landing
void * LandingJob(void * requestTime) {

  time_t request_time = * ((time_t * ) requestTime);
  Job job;
  
  job.ID = ID;
  job.type = 'L';
  job.requestTime = request_time;
  job.endTime = 0;
  job.turnAroundTime = 0;
  job.pad = '?';
  job.size = t;

  pthread_mutex_lock(&mutex);
  ID++;
  pthread_mutex_unlock(&mutex);

  pthread_mutex_lock(&landingMutex);
  Enqueue(landingQueue, job);
  pthread_mutex_unlock(&landingMutex);

  pthread_exit(0);

}

// the function that creates plane threads for departure
void * LaunchJob(void * requestTime) {

  time_t request_time = * ((time_t * ) requestTime);
  Job job;

  job.ID = ID;
  job.type = 'D';
  job.requestTime = request_time;
  job.endTime = 0;
  job.turnAroundTime = 0;
  job.pad = 'A';
  job.size = 2 * t;

  pthread_mutex_lock(&mutex);
  ID++;
  pthread_mutex_unlock(&mutex);

  pthread_mutex_lock(&launchingMutex);
  Enqueue(launchingQueue, job);
  pthread_mutex_unlock(&launchingMutex);

  //First land job signals to tower to start
  if (job.ID == 1) {
    pthread_cond_signal(&startCond);
  }

  pthread_exit(0);
}

// the function that creates plane threads for emergency landing
void * EmergencyJob(void * requestTime) {

  time_t request_time = * ((time_t * ) requestTime);
  Job job;

  job.ID = ID;
  job.type = 'E';
  job.requestTime = request_time;
  job.endTime = 0;
  job.turnAroundTime = 0;
  job.pad = '?';
  job.size = t;

  pthread_mutex_lock(&mutex);
  ID++;
  pthread_mutex_unlock(&mutex);

  pthread_mutex_lock(&emergencyMutex);
  Enqueue(emergencyQueue, job);
  pthread_mutex_unlock(&emergencyMutex);

  pthread_exit(0);
}

// the function that creates plane threads for assembly
void * AssemblyJob(void * requestTime) {

  time_t request_time = * ((time_t * ) requestTime);
  Job job;

  job.ID = ID;
  job.type = 'A';
  job.requestTime = request_time;
  job.endTime = 0;
  job.turnAroundTime = 0;
  job.pad = 'B';
  job.size = 6 * t;

  pthread_mutex_lock(&mutex);
  ID++;
  pthread_mutex_unlock(&mutex);

  pthread_mutex_lock(&assemblyMutex);
  Enqueue(assemblyQueue, job);
  pthread_mutex_unlock(&assemblyMutex);
  
  pthread_exit(0);
}

// the function that controls the air traffic
void * ControlTower(void * arg) {

  //Tower start signal
  pthread_cond_wait(&startCond, &start);

  //Singal to pad to start
  pthread_cond_signal(&padAStart);
  pthread_cond_signal(&padBStart);

  pthread_mutex_lock(&timeMutex);
  if (gettimeofday(&current_time, NULL) == -1)
    printf("Getting time of the day failed!.\n");
  pthread_mutex_unlock(&timeMutex);

  /*Main loop for tower. Tower does not sleep to be able to respond 
  as quick as possible when a job comes or goes. 
  In this way computer simulation runs without losing any seconds.*/
  while (current_time.tv_sec < endTime) {


    pthread_mutex_lock(&landingMutex);
    Job job = front(landingQueue);
    time_t requestTime = job.requestTime;
    pthread_mutex_unlock(&landingMutex);

    //If there is an emergency job then give it to the pads
    if (!isEmpty(emergencyQueue)) {

      Job j = front(emergencyQueue);
      pthread_mutex_unlock(&emergencyMutex);

      pthread_mutex_lock(&padAQueueMutex);
      pthread_mutex_lock(&padBQueueMutex);

      if (padAQueue -> size > padBQueue -> size) {

        j.pad = 'B';
        if (Enqueue(padBQueue, j)) {
          pthread_mutex_lock(&emergencyMutex);
          Dequeue(emergencyQueue);
          pthread_mutex_unlock(&emergencyMutex);
        }
      } else {
        j.pad = 'A';
        if (Enqueue(padAQueue, j)) {
          pthread_mutex_lock(&emergencyMutex);
          Dequeue(emergencyQueue);
          pthread_mutex_unlock(&emergencyMutex);
        }
      }
      pthread_mutex_unlock(&padAQueueMutex);
      pthread_mutex_unlock(&padBQueueMutex);

    //Else langing jobs
    } else if ((!isEmpty(landingQueue)) && ((current_time.tv_sec - startTime) != 80) && ((((current_time.tv_sec - startTime) - requestTime) >= 16) || ((launchingQueue -> size < 3) && (assemblyQueue -> size < 3)))) {
    
      pthread_mutex_lock(&padAQueueMutex);
      pthread_mutex_lock(&padBQueueMutex);
      pthread_mutex_lock(&landingMutex);

      Job job = front(landingQueue);
      time_t requestTime = job.requestTime;

      if (padAQueue -> size > padBQueue -> size) {

        job.pad = 'B';
        if (Enqueue(padBQueue, job)) {
          Dequeue(landingQueue);
        }
      } else {
        job.pad = 'A';
        if (Enqueue(padAQueue, job)) {
          Dequeue(landingQueue);
        }
      }

      pthread_mutex_unlock(&padAQueueMutex);
      pthread_mutex_unlock(&padBQueueMutex);
      pthread_mutex_unlock(&landingMutex);
    }
    //Else launching and assembly jobs
    else {
      if ((current_time.tv_sec - startTime) != 80) {
      
        pthread_mutex_lock(&launchingMutex);
        if (!isEmpty(launchingQueue)) {

          Job j = front(launchingQueue);
          pthread_mutex_lock(&padAQueueMutex);
          if (Enqueue(padAQueue, j)) {
            Dequeue(launchingQueue);
          }
          pthread_mutex_unlock(&padAQueueMutex);
        }
        pthread_mutex_unlock(&launchingMutex);

        pthread_mutex_lock(&assemblyMutex);
        if (!isEmpty(assemblyQueue)) {
          Job j = front(assemblyQueue);
          pthread_mutex_lock(&padBQueueMutex);
          if (Enqueue(padBQueue, j)) {
            Dequeue(assemblyQueue);
          }
          pthread_mutex_unlock(&padBQueueMutex);
        }
        pthread_mutex_unlock(&assemblyMutex);
      }
    }
    
    pthread_mutex_lock(&timeMutex);
    if (gettimeofday(&current_time, NULL) == -1)
      printf("Getting time of the day failed!.\n");
    pthread_mutex_unlock(&timeMutex);
    
  }
  pthread_exit(0);
}

void * PadA(void * arg) {

  //Wait for signal to start
  pthread_cond_wait(&padAStart, &startA);

  pthread_mutex_lock(&timeMutex);
  if (gettimeofday(&current_time, NULL) == -1)
    printf("Getting time of the day failed!.\n");
  pthread_mutex_unlock(&timeMutex);

  while (current_time.tv_sec < endTime) {

    if (!isEmpty(padAQueue)) {

      pthread_mutex_lock(&mutexA);

      pthread_mutex_lock(&padAQueueMutex);
      Job j = front(padAQueue);
      pthread_mutex_unlock(&padAQueueMutex);
      //Simulating the job
      pthread_sleep(j.size);

      pthread_mutex_lock(&timeMutex);
      if (gettimeofday(&current_time, NULL) == -1)
        printf("Getting time of the day failed!.\n");
      pthread_mutex_unlock(&timeMutex);

      if (current_time.tv_sec <= endTime) {
        pthread_mutex_lock(&padAQueueMutex);
        j = Dequeue(padAQueue);
        pthread_mutex_unlock(&padAQueueMutex);
        //Modifying logs
        j.endTime = (current_time.tv_sec - startTime);
        j.turnAroundTime = j.endTime - j.requestTime;
        //Writing logs
        fprintf(fp, "%3d       %c       %3ld        %3ld          %3ld            %c    %3d \n", j.ID, j.type, j.requestTime, j.endTime, j.turnAroundTime, j.pad, j.size);
      }
      pthread_mutex_unlock(&mutexA);
    } else {
      pthread_mutex_lock(&timeMutex);
      if (gettimeofday(&current_time, NULL) == -1)
        printf("Getting time of the day failed!.\n");
      pthread_mutex_unlock(&timeMutex);
    }
  }
  pthread_exit(0);
}

void * PadB(void * arg) {

  ////Wait for signal to start	
  pthread_cond_wait(&padBStart, &startB);

  pthread_mutex_lock(&timeMutex);
  if (gettimeofday(&current_time, NULL) == -1)
    printf("Getting time of the day failed!.\n");
  pthread_mutex_unlock(&timeMutex);

  while (current_time.tv_sec < endTime) {

    if (!isEmpty(padBQueue)) {

      pthread_mutex_lock(&mutexB);

      pthread_mutex_lock(&padBQueueMutex);
      Job j = front(padBQueue);
      pthread_mutex_unlock(&padBQueueMutex);
      //Simulating the job
      pthread_sleep(j.size);

      pthread_mutex_lock(&timeMutex);
      if (gettimeofday(&current_time, NULL) == -1)
        printf("Getting time of the day failed!.\n");
      pthread_mutex_unlock(&timeMutex);

      if (current_time.tv_sec <= endTime) {
        pthread_mutex_lock(&padBQueueMutex);
        j = Dequeue(padBQueue);
        pthread_mutex_unlock(&padBQueueMutex);
        //Modifying logs
        j.endTime = (current_time.tv_sec - startTime);
        j.turnAroundTime = j.endTime - j.requestTime;
        //Writing logs
        fprintf(fp, "%3d       %c       %3ld        %3ld          %3ld            %c    %3d \n", j.ID, j.type, j.requestTime, j.endTime, j.turnAroundTime, j.pad, j.size);
      }
      pthread_mutex_unlock(&mutexB);
    } else {
      pthread_mutex_lock(&timeMutex);
      if (gettimeofday(&current_time, NULL) == -1)
        printf("Getting time of the day failed!.\n");
      pthread_mutex_unlock(&timeMutex);
    }
  }
  pthread_exit(0);
}