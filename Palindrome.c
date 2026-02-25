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

//checks if word is a palindrome
int check_palindrome(char *word){
    int i = 0;
    int j = strlen(word) - 1;

    while(i < j){
        if(word[i] != word[j]){
            return 0;
        }
        i++;
        j--;
    }
    return 1;
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("args for Palindrome need to be 1 word for checking");
        exit(EXIT_FAILURE);
    }
    int read_fd = atoi(argv[1]);
    char mem_name[64];

    //FD from manager
    read(read_fd, mem_name, sizeof(mem_name));
    close(read_fd);

    printf("Palindrome process [%d]: starting.\n", getpid());

    int shm_fd = shm_open(mem_name, O_RDWR, 0666);
    if(shm_fd == -1){
        printf("failed shm_open");
        exit(EXIT_FAILURE);
    }

    //map shared memory
    shared_mem_t *shared_mem = mmap(0, sizeof(shared_mem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("failed mmap");
        exit(EXIT_FAILURE);
    }

    printf("Palindrome process [%d]: read word \"%s\" from shared memory\n", getpid(), shared_mem->word);
    int isValid = check_palindrome(shared_mem->word);

    if(isValid == 1){
        printf("Palindrome process [%d]: %s *IS* a palindrome\n", getpid(), shared_mem->word);
    }else if(isValid == 0){
        printf("Palindrome process [%d]: %s *IS NOT* a palindrome\n", getpid(), shared_mem->word);
    }

    shared_mem->result = isValid;
    printf("Palindrome process [%d]: wrote result (%d) to shared memory.\n", getpid(), isValid);

    munmap(shared_mem, sizeof(shared_mem_t));
    close(shm_fd);

    return 0;
}