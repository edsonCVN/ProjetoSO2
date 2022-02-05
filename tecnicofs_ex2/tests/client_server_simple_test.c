#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*  This test is similar to test1.c from the 1st exercise.
    The main difference is that this one explores the
    client-server architecture of the 2nd exercise. */

int main(int argc, char **argv) {

    char *str = "AAA!";
    char *path = "/f1";
    char buffer[40];

    char *str2 = "AAAg!";
    char *path2 = "/f12";
    char buffer2[40];

    int f = 0;
    ssize_t r = 0;

    int f2 ;
    ssize_t r2;

    int t;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    t = fork();

    if (t == 0) {
        assert(tfs_mount(argv[1], argv[3]) == 0);
        
        f = tfs_open(path, TFS_O_CREAT);
        assert(f != -1);

        r = tfs_write(f, str, strlen(str));
        assert(r == strlen(str));

        assert(tfs_close(f) != -1);

        f = tfs_open(path, 0);
        assert(f != -1);

        r = tfs_read(f, buffer, sizeof(buffer) - 1);
        assert(r == strlen(str));

        buffer[r] = '\0';
        assert(strcmp(buffer, str) == 0);

        assert(tfs_close(f) != -1);
        
        assert(tfs_unmount() == 0);

        printf("Successful test.\n");
        
    } else  {

        assert(tfs_mount(argv[2], argv[3]) == 0);
        
        f2 = tfs_open(path2, TFS_O_CREAT);
        assert(f2 != -1);

        r2 = tfs_write(f2, str2, strlen(str2));
        assert(r2 == strlen(str2));

        assert(tfs_close(f2) != -1);

        f2 = tfs_open(path2, 0);
        assert(f2 != -1);

        r2 = tfs_read(f2, buffer2, sizeof(buffer2) - 1);
        assert(r2 == strlen(str2));

        buffer2[r2] = '\0';
        assert(strcmp(buffer2, str2) == 0);

        assert(tfs_close(f2) != -1);
        
        assert(tfs_unmount() == 0);

        printf("Successful 02 test.\n");

        exit(0);

    }

    return 0;
}
