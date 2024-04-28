#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/shm.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "helpers.h"

// To prevent multiple students to define semaphore with the same name, 
// please always define the name of a semaphore ending with "_GROUP_NAME"
// where GROUP is your group number and NAME is your full name.
// For example, if you have 3 semaphores, you can define them as:
// semaphore_1_GROUP_NAME, semaphore_2_GROUP_NAME, semaphore_3_GROUP_NAME ...
#define PARAM_ACCESS_SEMAPHORE_1 "/semaphore_1_a_z"
#define PARAM_ACCESS_SEMAPHORE_2 "/semaphore_2_a_z"

// Limited buffer size of 1MB for shared memory.
#define BUFFER_SIZE (1024 * 1024) 
// Max files nums;
#define MAX_FILES 100


char* files[MAX_FILES];
int file_cnt = 0;

/**
 * @brief This function recursively traverse the source directory.
 * 
 * @param dir_name : The source directory name.
 */
void traverseDir(char *dir_name);

int main(int argc, char **argv) {
	int process_id; // Process identifier 
	int shmid, signal;
	char *shared_param_p, *shared_param_c;
	long int words_cnt = 0;
	
    // The source directory. 
    // It can contain the absolute path or relative path to the directory.
	char *dir_name = argv[1];

	if (argc < 2) {
		printf("Main process: Please enter a source directory name.\nUsage: ./main <dir_name>\n");
		exit(-1);
	}

	traverseDir(dir_name);

    /////////////////////////////////////////////////
    // You can add some code here to prepare before fork.
    /////////////////////////////////////////////////
	sem_unlink(PARAM_ACCESS_SEMAPHORE_1);
	sem_unlink(PARAM_ACCESS_SEMAPHORE_2);
	sem_t *read_sem = sem_open(PARAM_ACCESS_SEMAPHORE_1, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 0);
	sem_t *write_sem = sem_open(PARAM_ACCESS_SEMAPHORE_2, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);
	shmid = shmget(IPC_PRIVATE, BUFFER_SIZE, 0666|IPC_CREAT);

	switch (process_id = fork()) {

	default:
		/*
			Parent Process
		*/
		printf("Parent process: My ID is %jd\n", (intmax_t) getpid());

        /////////////////////////////////////////////////
        // Implement your code for parent process here.
        /////////////////////////////////////////////////
		shared_param_p = (char *) shmat(shmid, 0, 0);
		for(int i = 0; i < file_cnt; ++i) {
			FILE *file = fopen(files[i], "r");
			if(file == NULL) {
				fprintf(stderr, "Failed to open file %s\n", files[i]);
				exit(-1);
			}
			long len = fileLength(file);
			// If file size exceeds buffer size, split the file content into several chunks.
			while(len > 0) {
				sem_wait(write_sem);
				long size = len < BUFFER_SIZE ? len : BUFFER_SIZE;
				fread(shared_param_p, sizeof(char), size, file);
				shared_param_p[size] = '\0';
				len -= size;
				sem_post(read_sem);	
			}
			fclose(file);
		}
		sem_wait(write_sem);
		shared_param_p[0] = '\0';
		sem_post(read_sem);
		wait(&signal);

		shmdt(shared_param_p);
		shmctl(shmid, IPC_RMID, 0);
		// Close and delete semaphore. 
        sem_close(read_sem);
		sem_close(write_sem);
        sem_unlink(PARAM_ACCESS_SEMAPHORE_1);
		sem_unlink(PARAM_ACCESS_SEMAPHORE_2);
		printf("Parent process: Finished.\n");
		break;

	case 0:
		/*
			Child Process
		*/

		printf("Child process: My ID is %jd\n", (intmax_t) getpid());

        /////////////////////////////////////////////////
        // Implement your code for child process here.
        /////////////////////////////////////////////////
		while(1) {
			sem_wait(read_sem);
			shared_param_c = (char *) shmat(shmid, 0, 0);
			int words = wordCount(shared_param_c);
			if(!words) break;
			words_cnt += words;
			sem_post(write_sem);
		}
		// Save the result.
		saveResult("p2_result.txt", words_cnt);
		shmdt(shared_param_c);
		printf("Child process: Finished.\n");
		exit(0);

	case -1:
		/*
		Error occurred.
		*/
		printf("Fork failed!\n");
		exit(-1);
	}
	exit(0);
}

/**
 * @brief This function recursively traverse the source directory.
 * 
 * @param dir_name : The source directory name.
 */
void traverseDir(char *dir_name){
	DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open dir %s\n", dir_name);
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL && file_cnt < MAX_FILES) {
		// Ignore . / ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
		// +2, one for '/', and one for '\0'.
		char *path = (char *)malloc(strlen(dir_name) + strlen(entry->d_name) + 2);
		sprintf(path, "%s/%s", dir_name, entry->d_name);
		if(entry->d_type == DT_DIR) {
			traverseDir(path);
		} else if(entry->d_type == DT_REG && validateTextFile(entry->d_name)) {
            files[file_cnt] = path;
            file_cnt++;
        }
    }
    closedir(dir);
}