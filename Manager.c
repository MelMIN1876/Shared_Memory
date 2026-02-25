#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct{
    int result;
    char word[4092];
} shared_mem_t;

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("Program needs >0 words for Palindrome verification");
        exit(EXIT_FAILURE);
    }
    int num_words = argc - 1;
    //allocate memory for pids
    pid_t *pids = malloc(num_words * sizeof(pid_t));

    if(pids == NULL){
        printf("failed malloc for pids");
        exit(EXIT_FAILURE);
    }
    //allocate memory for names
    char **mem_names = malloc(num_words * sizeof(char *));

    if(mem_names == NULL){
        printf("failed malloc for mem_names");
        exit(EXIT_FAILURE);
    }

    //allocate memory for names[i]
    for(int i = 0; i < num_words; i++){
        mem_names[i] = malloc(64 * sizeof(char));
        if(mem_names[i] == NULL){
            printf("failed malloc for %d name in mem_names", i);
            exit(EXIT_FAILURE);
        }
    }

    //loops (number of words) times
    for(int i = 0; i < num_words; i++){
        int pipefd[2];
        if(pipe(pipefd) == -1){
            printf("pipe failure\n");
            exit(EXIT_FAILURE);
        }

        sprintf(mem_names[i], "/mem_%d_%d", getpid(), i);
        //make shared memory segment
        int shm_fd = shm_open(mem_names[i], O_CREAT | O_RDWR, 0666);
        if(shm_fd == -1){
            printf("shm_open failed\n");
            exit(EXIT_FAILURE);
        }

        //configure size of shared memory object
        int trunc = ftruncate(shm_fd, sizeof(shared_mem_t));
        if(trunc == -1){
            printf("error truncating");
            exit(EXIT_FAILURE);
        }

        //memory map the shared memory object
        shared_mem_t *shared_mem = mmap(0, sizeof(shared_mem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if(shared_mem == MAP_FAILED){
            printf("mmap failed");
            exit(EXIT_FAILURE);
        }

        //Make initial shared mem values
        shared_mem->result = -1;
        strncpy(shared_mem->word, argv[i + 1], sizeof(shared_mem->word) - 1);
        shared_mem->word[sizeof(shared_mem->word) - 1] = '\0';

        //forking starts
        pid_t pid = fork();
        if(pid < 0){
            printf("fork failed");
            exit(EXIT_FAILURE);
        }else if(pid == 0){
            //child
            close(pipefd[1]); //closes write end
            char read_fd[64];
            sprintf(read_fd, "%d", pipefd[0]);

            execlp("./palindrome", "./palindrome", read_fd, (char *)NULL);

            exit(1);
        }else if(pid > 0){
            //parent
            close(pipefd[0]); //closes read end of pipe
            write(pipefd[1], mem_names[i], strlen(mem_names[i]) + 1);
            close(pipefd[1]);

            printf("Manager: forked process with ID %d.\n", pid);
            printf("Manager: wrote shm name to pipe (%zu bytes)\n", strlen(mem_names[i]) + 1);
            pids[i] = pid;

            //unmap and close shm fd
            munmap(shared_mem, sizeof(shared_mem_t));
            close(shm_fd);
        }
    }

    //start waiting for all child processes
    for(int i = 0; i < num_words; i++){
        printf("Manager: waiting on child process ID %d...\n", pids[i]);
        waitpid(pids[i], NULL, 0);

        //open shared memory object (mem_names[i])
        int shm_fd = shm_open(mem_names[i], O_RDONLY, 0666);
        if(shm_fd == -1){
            printf("failed shm_open");
            exit(EXIT_FAILURE);
        }

        //map the shared memory object
        shared_mem_t *shared_mem = mmap(0, sizeof(shared_mem_t), PROT_READ, MAP_SHARED, shm_fd, 0);
        if(shared_mem == MAP_FAILED){
            printf("failed mmap");
            exit(EXIT_FAILURE);
        }
        
        //check results
        if(shared_mem->result == 1){
            printf("Manager: result 1 read from shared memory. \"%s\" is a palindrome.\n", shared_mem->word);
        }else if(shared_mem->result == 0){
            printf("Manager: result 0 read from shared memory. \"%s\" is not a palindrome.\n", shared_mem->word);
        }else{
            printf("failed reading, something is wrong\n");
        }

        //clean up
        munmap(shared_mem, sizeof(shared_mem_t));
        close(shm_fd);
        shm_unlink(mem_names[i]);
    }
    
    //free memory
    for(int i = 0; i < num_words; i++){
        if(mem_names[i] != NULL){
            free(mem_names[i]);
        }
    }
    if(mem_names != NULL){
        free(mem_names);
    }
    if(pids != NULL){
        free(pids);
    }

    printf("Manager: exiting.\n");
    return 0;

}