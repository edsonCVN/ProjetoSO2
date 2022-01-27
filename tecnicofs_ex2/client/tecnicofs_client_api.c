#include "tecnicofs_client_api.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

int session_id = -1;
int tx_server_pipe = -1;
int rx_client_pipe = -1;
char c_pipe_path[PIPE_PATH_SIZE];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    
    char intput[CLIENT_BUFFER_SIZE];
    //int output = -1;
    int output_ptr;
    char *num;

    //buffer's initialization
    memset(intput, '\0', CLIENT_BUFFER_SIZE);

    if (asprintf(&num, "%d", TFS_OP_CODE_UNMOUNT) == -1) {
        perror("asprintf");
    } else {
        strcat(strcpy(intput, num), "|");
        strcat(intput, client_pipe_path);
        printf("%s\n", intput);
    }

    //memcpy(intput, TFS_OP_CODE_MOUNT + "|", 2);
    //memcpy(intput + 2, client_pipe_path, strlen(client_pipe_path));
    memcpy(c_pipe_path, client_pipe_path, strlen(client_pipe_path));
    c_pipe_path[strlen(client_pipe_path)] = 0;


    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }

    // create pipe
    if (mkfifo(client_pipe_path, 0640) != 0) {
        return -1;
    }

    // first open clients->server pipe for write 
    int tx_server_pipe = open(server_pipe_path, O_WRONLY);
    if (tx_server_pipe == -1) {
        return -1;
    }
    
    if(write(tx_server_pipe, intput, CLIENT_BUFFER_SIZE) < 0) {
        return -1;
    }
    
    //assumindo que já foi aberto para escrita no server
    int rx_client_pipe = open(client_pipe_path, O_RDONLY);
    if(rx_client_pipe == -1) {
        return -1;
    }
    
    //if(read(rx_client_pipe, output, sizeof(int)) < 0 || output < 0) { //posso ler diretamente para um inteiro????? e ver logo o output???
    if(read(rx_client_pipe, &output_ptr, sizeof(int)) < 0 || output_ptr < 0) {
        return -1;
    }

    //session_id = output;
    session_id = output_ptr;

    return 0;
}

int tfs_unmount() {
    
    char intput[CLIENT_BUFFER_SIZE];
    int output = -1;
    char *num;
    char *num1;

    //buffer's initialization
    memset(intput, '\0', CLIENT_BUFFER_SIZE);

    if (asprintf(&num1, "%d", TFS_OP_CODE_UNMOUNT) == -1 || asprintf(&num, "%d", session_id) == -1 ) {
        perror("asprintf");
    } else {
        strcat(strcpy(intput, num1), "|");
        strcat(intput, num);
    }

    //memcpy(intput, TFS_OP_CODE_UNMOUNT + "|", 2);
    //memcpy(intput + 2, session_id, sizeof(int));

    if(write(tx_server_pipe, intput, CLIENT_BUFFER_SIZE) < 0) {
        return -1;
    }

    if(read(rx_client_pipe, output, sizeof(int)) < 0 || output < 0) { //posso ler diretamente para um inteiro????? e ver logo o output???
        return -1;
    }

    close(rx_client_pipe);
    close(tx_server_pipe);
    
    if (unlink(c_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }
    return 0;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
