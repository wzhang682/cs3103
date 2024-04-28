#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/shm.h>		// This is necessary for using shared memory constructs
#include <semaphore.h>		// This is necessary for using semaphore
#include <fcntl.h>			// This is necessary for using semaphore
#include <pthread.h>        // This is necessary for Pthread          
#include <string.h>
#include "helpers.h"

// To prevent multiple students to define semaphore with the same name, 
// please always define the name of a semaphore ending with "_GROUP_NAME"
// where GROUP is your group number and NAME is your full name.
// For example, if you have 3 semaphores, you can define them as:
// semaphore_1_GROUP_NAME, semaphore_2_GROUP_NAME, semaphore_3_GROUP_NAME ...
#define PARAM_ACCESS_SEMAPHORE "/semaphore_1_a_z"

long int global_param = 0;
int num_of_operations = 0;
int digits[9];

// Define all the semaphores.
sem_t sems[9];

/**
* This function should be implemented by yourself. It must be invoked
* in the child process after the input parameter has been obtained.
* @parms: The input parameter from the terminal.
*/
void multi_threads_run(long int input_param);

/**
 * @brief This function will be executed by all spawned threads to
 * do addition operation.
 * @param arg 
 */
void *addition_operation(void *arg);

int main(int argc, char **argv)
{
	int shmid, status;
	long int local_param = 0;
	long int *shared_param_p, *shared_param_c;

	if (argc < 3) {
		printf("Please enter a nine-digit decimal number as the input parameter \n \
and a number as the times the executing the addition operation for each thread.\n \
Usage: ./main <input_param> <num_of_operations>\n");
		exit(-1);
	}
	num_of_operations = strtol(argv[2], NULL, 10);

   	/*
		Creating semaphores. Mutex semaphore is used to acheive mutual
		exclusion while processes access (and read or modify) the global
		variable, local variable, and the shared memory.
	*/ 

	// Checks if the semaphore exists, if it exists we unlink him from the process.
	sem_unlink(PARAM_ACCESS_SEMAPHORE);
	
	// Create the semaphore. sem_init() also creates a semaphore. Learn the difference on your own.
	sem_t *param_access_semaphore = sem_open(PARAM_ACCESS_SEMAPHORE, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);

	// Check for error while opening the semaphore
	if (param_access_semaphore != SEM_FAILED){
		printf("Successfully created new semaphore!\n");
	}	
	else if (errno == EEXIST) {   // Semaphore already exists
		printf("Semaphore appears to exist already!\n");
		param_access_semaphore = sem_open(PARAM_ACCESS_SEMAPHORE, 0);
	}
	else {  // An other error occured
		assert(param_access_semaphore != SEM_FAILED);
		exit(-1);
	}

	/*  
	    Creating shared memory. 
        The operating system keeps track of the set of shared memory
	    segments. In order to acquire shared memory, we must first
	    request the shared memory from the OS using the shmget()
      	system call. The second parameter specifies the number of
	    bytes of memory requested. shmget() returns a shared memory
	    identifier (SHMID) which is an integer. Refer to the online
	    man pages for details on the other two parameters of shmget()
	*/
	shmid = shmget(IPC_PRIVATE, sizeof(long int), 0666|IPC_CREAT); // We request an array of one long integer

	/* 
	    After forking, the parent and child must "attach" the shared
	    memory to its local data segment. This is done by the shmat()
	    system call. shmat() takes the SHMID of the shared memory
	    segment as input parameter and returns the address at which
	    the segment has been attached. Thus shmat() returns a char
	    pointer.
	*/

	if (fork() == 0) { // Child Process
        
		printf("Child Process: Child PID is %jd\n", (intmax_t) getpid());
		
		/*  shmat() returns a long int pointer which is typecast here
		    to long int and the address is stored in the long int pointer shared_param_c. */
        shared_param_c = (long int *) shmat(shmid, 0, 0);

		while (1) // Loop to check if the variables have been updated.
		{
			// Get the semaphore
			sem_wait(param_access_semaphore);
			printf("Child Process: Got the variable access semaphore.\n");

			if ( (global_param != 0) || (local_param != 0) || (shared_param_c[0] != 0) )
			{
				printf("Child Process: Read the global variable with value of %ld.\n", global_param);
				printf("Child Process: Read the local variable with value of %ld.\n", local_param);
				printf("Child Process: Read the shared variable with value of %ld.\n", shared_param_c[0]);

                // Release the semaphore
                sem_post(param_access_semaphore);
                printf("Child Process: Released the variable access semaphore.\n");
                
				break;
			}
			// Release the semaphore
			sem_post(param_access_semaphore);
			printf("Child Process: Released the variable access semaphore.\n");
		}
        /**
         * After you have fixed the issue in Problem 1-Q1, 
         * uncomment the following multi_threads_run function 
         * for Problem 1-Q2. Please note that you should also
         * add an input parameter for invoking this function, 
         * which can be obtained from one of the three variables,
         * i.e., global_param, local_param, shared_param_c[0].
         */
		multi_threads_run(shared_param_c[0]);

		/* each process should "detach" itself from the 
		   shared memory after it is used */

		shmdt(shared_param_c);

		exit(0);
	}
	else { // Parent Process

		printf("Parent Process: Parent PID is %jd\n", (intmax_t) getpid());

		/*  shmat() returns a long int pointer which is typecast here
		    to long int and the address is stored in the long int pointer shared_param_p.
		    Thus the memory location shared_param_p[0] of the parent
		    is the same as the memory locations shared_param_c[0] of
		    the child, since the memory is shared.
		*/
		shared_param_p = (long int *) shmat(shmid, 0, 0);

		// Get or lock the semaphore first
		sem_wait(param_access_semaphore);
		printf("Parent Process: Got the variable access semaphore.\n");

		global_param = strtol(argv[1], NULL, 10);
		local_param = strtol(argv[1], NULL, 10);
		shared_param_p[0] = strtol(argv[1], NULL, 10);

		// Release or unlock the semaphore
		sem_post(param_access_semaphore);
		printf("Parent Process: Released the variable access semaphore.\n");
        
		wait(&status);

		/* each process should "detach" itself from the 
		   shared memory after it is used */

		shmdt(shared_param_p);

		/* Child has exited, so parent process should delete
		   the created shared memory. Unlike attach and detach,
		   which is to be done for each process separately,
		   deleting the shared memory has to be done by only
		   one process after making sure that noone else
		   will be using it 
		 */

		shmctl(shmid, IPC_RMID, 0);

        // Close and delete semaphore. 
        sem_close(param_access_semaphore);
        sem_unlink(PARAM_ACCESS_SEMAPHORE);

		exit(0);
	}

	exit(0);
}

/**
* This function should be implemented by yourself. It must be invoked
* in the child process after the input parameter has been obtained.
* @parms: The input parameter from terminal.
*/
void multi_threads_run(long int input_param)
{
	long int result = 0;
	pthread_t threads[9];
	for(int i = 0; i < 9; ++i) {
		digits[i] = input_param%10;
		input_param /= 10;
	}
	// Init all semaphores.
	for(int i = 0; i < 9; ++i) {
		sem_init(&sems[i], 0, 1);
	}
	
	// Init all threads.
	for(int i = 0; i < 9; ++i) {
		int *index = malloc(sizeof(int));
		*index = i;
		pthread_create(&threads[i], NULL, addition_operation, index);
	}

	// Wait for all threads to terminate.
	for(int i = 0; i < 9; ++i) {
		pthread_join(threads[i], NULL);
	}

	// Close all semaphores.
	for(int i = 0; i < 9; ++i) {
		sem_destroy(&sems[i]);
	}

	// Save the result.
	for(int i = 0, radis = 1; i < 9; ++i, radis*=10) {
		result += digits[i]*radis;
	}
	saveResult("p1_result.txt", result);
}


void *addition_operation(void *arg) {
	int index = *(int*)arg;
	int iter = 0;
	while(iter < num_of_operations) {
		// Try to get both semaphores
		if (!sem_trywait(&sems[index])) {
			if(!sem_trywait(&sems[(index + 1) % 9])) {
				// Addition operation.
				digits[index] = (digits[index] + 1) % 10;
				digits[(index + 1) % 9] = (digits[(index + 1) % 9] + 1) % 10;

				// Post semaphores.
				sem_post(&sems[index]);
				sem_post(&sems[(index + 1) % 9]);
				iter++;
			}
			sem_post(&sems[index]);
		}
	}
}
