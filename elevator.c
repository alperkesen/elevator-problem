
/************************************************************

    Name:        Ekrem Alper Kesen
    ID:          150150018
    Course:      Bilgisayar İşletim Sistemleri
    Assignment:  3

*************************************************************/


//-------------------------LIBRARIES-------------------------

#define _GNU_SOURCE

#include <stdio.h>     // printf
#include <stdlib.h>    // exit
#include <string.h>    // strerror
#include <error.h>
#include <time.h>

#include <unistd.h>    // fork, exec
#include <errno.h>     // errno
#include <sys/wait.h>  // wait

#include <sys/ipc.h>
#include <sys/types.h>

#include <sys/shm.h>   // shared memory
#include <sys/sem.h>   // semaphores

#include <signal.h>    // handling signals

//------------------------DEFINE--------------------------------


#define KEYSEM           ftok(get_current_dir_name(), 1000)
#define KEYSHM           ftok(get_current_dir_name(), 10000)

#define SIMULATION_TIME  60


//---------------------FUNCTION PROTOTYPES----------------------


void sem_signal(int, int);
void sem_wait(int, int);

void mysignal(int);
void mysigset(int);

void call_elevator(int, int, int);
void move_elevator(int, int, int);

typedef enum {WAIT, UP, DOWN} status;


//----------------------------MAIN-------------------------------

int main(int argc, char *argv[]) {
  if (argc < 5) {
    printf("Gerekli parametreleri giriniz.(N M K T)\n");
    return -1;
  }

  mysigset(12);

  int num_floor = atoi(argv[1]);      // N
  int elevator_cap = atoi(argv[2]);   // M
  int max_people = atoi(argv[3]);     // K
  int max_wandering = atoi(argv[4]);  // T

  int i;                              // Index
  int f;                              // Return value of fork
  int person[max_people];             // Child process ids
  int *floors;                        // array for requests in floors
  
  int shmid = 0;                      // Shared memory ID
  int lock;                           // Semaphore id
  int request = 0;

  time_t start;                       // Starting time

  status elevator_status;             // Status of the elevator
  status prev_status;                 // Previous status of the elevator
  int elevator_floor;                 // Current elevator floor
  
  int current_floor;                  // Current floor of person
  int target_floor;                   // Target floor of person
  int time_stay;                      // Waiting time of person


  // Printing the arguments

  printf("\n");
  
  printf("Binanın sahip olduğu kat sayısı: %d\n", num_floor);
  printf("Asansör kapasitesi: %d\n", elevator_cap);
  printf("Binada yer alan maksimum kişi sayısı: %d\n", max_people);
  printf("Tüm kişiler için maksimum dolaşma süresi: %d\n", max_wandering);

  printf("\n");

  elevator_floor = 1;
  elevator_status = WAIT;
  printf("Başlangıç durumu: asansör %d.katta\n", elevator_floor);
    

  // Create child processes for each person
  
  for (i = 0; i < max_people; i++) {
    f = fork();

    if (f == -1) {
      printf("FORK error...\n");
      exit(1);
    }

    if (f == 0) {
      break;
    }

    person[i] = f;
  }

  if (f != 0) {
    // Shared memory of requests 
    
    shmid = shmget(KEYSHM, sizeof(int) * num_floor, 0700|IPC_CREAT);
    
    if (shmid == -1) {
      printf("Shared memory error!\n");
      exit(1);
    }

    floors = (int *) shmat(shmid, 0, 0);

    for (i = 0; i < num_floor; i++) {
      floors[i] = 0;
    }
    
    shmdt(floors);

    // Create semaphore for each floor

    for (i = 0; i < num_floor; i++) {
      request = semget(ftok(get_current_dir_name(), (i + 1) * 10), 1, 0700|IPC_CREAT);
      semctl(request, 0, SETVAL, 0);
    }

    // Lock semaphore for mutex
    
    lock = semget(KEYSEM, 1, 0700|IPC_CREAT);
    semctl(lock, 0, SETVAL, 1);
    
    // Startin time of the simulation

    start = time(NULL);

    // Starting the child processes
      
    for (i = 0; i < max_people; i++) {
      kill(person[i], 12);
    }

    // Waiting for two seconds...

    sleep(2);


    // Elevator process
    
    while (time(NULL) - start < SIMULATION_TIME) {
      // Keeping the previous status (UP, DOWN) of the elevator
      
      prev_status = elevator_status;

      // Attaching the shared memory of requests for each floor
      
      int shmid = shmget(KEYSHM, sizeof(int) * num_floor, 0);
    
      if (shmid == -1) {
	printf("Shared memory error!\n");
	exit(1);
      }
      
      int *floors = (int *) shmat(shmid, 0, 0);


      // Deciding to go up or down
      
      if (elevator_status == UP) {
	elevator_status = DOWN;
	
	for (i = elevator_floor; i < num_floor; i++) {
	  if (floors[i] > 0) {
	    elevator_status = UP;
	    break;
	  }
	}
      } else {
	elevator_status = UP;
	
	for (i = elevator_floor - 1; i > 0; i--) {	  
	  if (floors[i - 1] > 0) {
	    elevator_status = DOWN;
	    break;
	  }
	}
      }

      /*

	For printing elevator floor, status (UP/DOWN), number of requests and all requests:


	printf("Asansör: %d (%d - %d)", elevator_floor, elevator_status, floors[elevator_floor-1]);
	printf("\t\t\t\t\t\tDurumlar:\t");
      
	for (i = 0; i < num_floor; i++) {
	  printf("%d, ", floors[i]);
	}
      
        printf("\n");
      */

  
      /*
	Getting the semaphore for current elevator floor,
	signaling it for number of requests times to make people
        go in to the elevator or get out from the elevator.
      */
      

      sem_wait(lock, 1);
      
      if (floors[elevator_floor - 1] > 0) {
	request = semget(ftok(get_current_dir_name(), elevator_floor * 10),
			   1, 0);
	int num_request = floors[elevator_floor - 1];
	
	for (i = 0; i < num_request; i++) {
	  sem_signal(request, 1);	  
	  floors[elevator_floor - 1]--;
	}
      }
      
      sem_signal(lock, 1);
      
      sleep(1);


      // Deciding to go up or down

      if (elevator_status == UP) {
	elevator_status = DOWN;
	
	for (i = elevator_floor; i < num_floor; i++) {
	  if (floors[i] > 0) {
	    elevator_status = UP;
	    break;
	  }
	}
      } else {
	elevator_status = UP;
	
	for (i = elevator_floor - 1; i > 0; i--) {	  
	  if (floors[i - 1] > 0) {
	    elevator_status = DOWN;
	    break;
	  }
	}
      }

      // If there is no request, don't move
      
      int no_request = 0;

      for (i = 0; i < num_floor; i++) {	  
	if (floors[i] > 0) {
	  no_request++;
	}
      }

      if (!no_request) {
	elevator_status = WAIT;
      }
      

      // Elevator is going up, down or waits.
      // If it changes direction, it prints.
      
      if (elevator_status == UP && elevator_floor < num_floor) {
	if (prev_status != UP) {
	  printf("Asansör yukarı çıkıyor.\n");
	}
	
	elevator_floor++;
      } else if (elevator_status == DOWN && elevator_floor > 1) {
	if (prev_status != DOWN) {
	  printf("Asansör aşağı iniyor.\n");
	}
	
	elevator_floor--;
      }
      
      sleep(1);
    }

 
    // Kill child processes
    
    for (i = 0; i < max_people; i++) {
      kill(person[i], SIGTERM);
    }

    wait(NULL);
 
    printf("Süre doldu, asansör ilk durumuna geçti.\n");


    // Remove semaphores
    
    semctl(shmid, 0, IPC_RMID, 0);
    semctl(lock, 0, IPC_RMID, 0);

    for (i = 0; i < num_floor; i++) {
      request = semget(ftok(get_current_dir_name(), (i + 1) * 10), 1, 0);
      semctl(request, 0, IPC_RMID, 0);
    }

    exit(0);
  } else {
    pause();

    srand(getpid());

    // Starting time
    
    start = time(NULL);

    current_floor = rand() % num_floor + 1;

    while (time(NULL) - start < SIMULATION_TIME) {
      time_stay = rand() % max_wandering;
      sleep(time_stay);

      target_floor = rand() % num_floor + 1;

      // To make sure current floor and target floor is not same
      
      while (target_floor == current_floor) {
	target_floor = rand() % num_floor + 1;
      }

      /*
	For printing the current floor, target floor and waiting time:


        printf("P%d:\t %d -> %d (%d)\n",
	       i+1, current_floor, target_floor, time_stay);
      */

      call_elevator(i, current_floor, num_floor);
      move_elevator(i, target_floor, num_floor);

      current_floor = target_floor;
    }

    exit(0);
  }
  
  return 0;
}


void sem_signal(int semid, int val) {
  struct sembuf semaphore;
  
  semaphore.sem_num = 0;
  semaphore.sem_op = val;
  semaphore.sem_flg = 1;

  semop(semid, &semaphore, 1);
}


void sem_wait(int semid, int val) {
  struct sembuf semaphore;

  semaphore.sem_num = 0;
  semaphore.sem_op = (-1) * val;
  semaphore.sem_flg = 1;

  semop(semid, &semaphore, 1);
}


void mysignal(int signum) {
  // printf("Received signal with num = %d.\n", signum);
}


void mysigset(int num) {
  struct sigaction mysigaction;

  mysigaction.sa_handler = (void *) mysignal;
  mysigaction.sa_flags = 0;

  sigaction(num, &mysigaction, NULL); 
}


void call_elevator(int i, int from_floor, int num_floor) {
  // For mutex
  
  int lock = semget(KEYSEM, 1, 0);
  sem_wait(lock, 1);

  // Attach the shared memory
  
  int shmid = shmget(KEYSHM, sizeof(int) * num_floor, 0);
    
  if (shmid == -1) {
    printf("Shared memory error!\n");
    exit(1);
  }

  int *floors = (int *) shmat(shmid, 0, 0);

  printf("P%d asansörü %d.kata çağırdı.\n", i + 1, from_floor);

  // Increase the number of requests for the current floor
  
  floors[from_floor - 1]++;

  shmdt(floors);
  sem_signal(lock, 1);

  int request = semget(ftok(get_current_dir_name(), from_floor * 10),
		       1, 0);

  sem_wait(request, 1);


  printf("Asansör %d.kattan P%d kişisini aldı. ", from_floor, i+1);
}


void move_elevator(int i, int to_floor, int num_floor) {
  // For mutex
  
  int lock = semget(KEYSEM, 1, 0);
  sem_wait(lock, 1);

  // Attach the shared memory
    
  int shmid = shmget(KEYSHM, sizeof(int) * num_floor, 0);
    
  if (shmid == -1) {
    printf("Shared memory error!\n");
    exit(1);
  }

  int *floors = (int *) shmat(shmid, 0, 0);

  printf("P%d, %d.kata bastı.\n", i + 1, to_floor);

  // Increase the number of requests for the target floor
  
  floors[to_floor - 1]++;

  shmdt(floors);
  sem_signal(lock, 1);


  int request = semget(ftok(get_current_dir_name(), to_floor * 10),
		       1, 0);

  sem_wait(request, 1);
  
  printf("Asansör P%d kişisini %d.kata bıraktı.\n", i + 1, to_floor);
}
