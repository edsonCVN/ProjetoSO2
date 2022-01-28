#include "fs/operations.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define SIZE 1000
/*  Simple test to check whether the implementation of
    tfs_destroy_after_all_closed is correct.
    Note: This test uses TecnicoFS as a library, not
    as a standalone server.
    We recommend trying more elaborate tests of tfs_destroy_after_all_closed.
    Also, we suggest trying out a similar test once the
    client-server version is ready (calling the tfs_shutdown_after_all_closed 
    operation).
*/

int f;

char input[SIZE];

char output[SIZE];
    

void *fn_thread_write(void *arg) {
    (void)
        arg; /* Since arg is not used, this line prevents a compiler warning */
    
    f = tfs_open("/f1", TFS_O_CREAT);
    assert(f != -1);

    //sleep(3);

    assert(tfs_write(f, input, SIZE) == SIZE);

    assert(tfs_close(f) != -1);

    return NULL;
}

void *fn_thread_read(void *arg) {
    (void)
        arg; /* Since arg is not used, this line prevents a compiler warning */
    
    f = tfs_open("/f1", TFS_O_CREAT);
    assert(f != -1);
    //sleep(3);

    assert(tfs_read(f, output, SIZE) == SIZE);
    printf("%.1000s\n", output);

    assert(tfs_close(f) != -1);

    return NULL;
}

void *fn_thread_write_2(void *arg) {
    (void)
        arg; /* Since arg is not used, this line prevents a compiler warning */
    
    f = tfs_open("/f1", TFS_O_CREAT);
    assert(f == -1);

    assert(tfs_write(f, input, SIZE) == -1);

    assert(tfs_close(f) == -1);

    return NULL;
}

void *fn_thread_read_2(void *arg) {
    (void)
        arg; /* Since arg is not used, this line prevents a compiler warning */
    
    f = tfs_open("/f1", TFS_O_CREAT);
    assert(f == -1);

    assert(tfs_read(f, output, SIZE) == -1);
    printf("%.1000s\n", output);

    assert(tfs_close(f) == -1);

    return NULL;
}

int main() {

    memset(input, 'A', SIZE - 1);
    memset(input + (SIZE - 1), 'X', 1);
    
    assert(tfs_init() != -1);

    pthread_t t[4];
    
    assert(pthread_create(&t[0], NULL, fn_thread_write, NULL) == 0);
    assert(pthread_create(&t[1], NULL, fn_thread_read, NULL) == 0);

    sleep(5);

    assert(tfs_destroy_after_all_closed() != -1);

    pthread_join(t[0], NULL);
    pthread_join(t[1], NULL);

    assert(pthread_create(&t[2], NULL, fn_thread_write_2, NULL) != -1);
    assert(pthread_create(&t[3], NULL, fn_thread_read_2, NULL) != -1);
    
    // No need to join thread
    printf("Successful test.\n");

    return 0;
}
