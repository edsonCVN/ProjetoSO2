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
    
    char input[40];
    char input1[2];
    int output = -1;

    memset(input, '\0', PIPE_PATH_SIZE);
    memcpy(input, client_pipe_path, strlen(client_pipe_path));

    memset(input1, '\0', 2);
    sprintf(input1, "%d", TFS_OP_CODE_MOUNT);
    
    printf("input1: %.1s\n", input1);
    printf("input: %s\n", input);
    
/*
    //buffer's initialization
    memset(input, '\0', 41);
    sprintf(input, "%c%s", TFS_OP_CODE_MOUNT, client_pipe_path);
    memcpy(c_pipe_path, client_pipe_path, strlen(client_pipe_path));
    c_pipe_path[strlen(client_pipe_path)] = 0;*/


    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }

    // create pipe
    if (mkfifo(client_pipe_path, 0640) != 0) {
        return -1;
    }

    // first open clients->server pipe for write 
    tx_server_pipe = open(server_pipe_path, O_WRONLY);
    if (tx_server_pipe == -1) {
        return -1;
    }
    printf("client buffer: %s\n", input);
    /*
    if(write(tx_server_pipe, input, 41) < 0) {
        return -1;
    }*/

    write(tx_server_pipe, input1, 1);
    write(tx_server_pipe, input, PIPE_PATH_SIZE);
    
    //assumindo que já foi aberto para escrita no server
    rx_client_pipe = open(client_pipe_path, O_RDONLY);
    if(rx_client_pipe == -1) {
        return -1;
    }
    
    //if(read(rx_client_pipe, output, sizeof(int)) < 0 || output < 0) { //posso ler diretamente para um inteiro????? e ver logo o output???
    if(read(rx_client_pipe, &output, sizeof(int)) < 0 || output < 0) {
        return -1;
    }
    
    //session_id = output;
    session_id = output;
    printf("recebeu session_id: %d\n", session_id);

    return 0;
}

int tfs_unmount() {
    
    char input[2];
    char input1[2];
    int output = -1;

    //buffer's initialization
    memset(input, '\0', 2);
    sprintf(input, "%d", TFS_OP_CODE_UNMOUNT);
    memset(input1, '\0', 2);
    sprintf(input1, "%d", session_id);
   
    if(write(tx_server_pipe, input, 1) < 0) {
        return -1;
    }

    if(write(tx_server_pipe, input1, 1) < 0) {
        return -1;
    }

    if(read(rx_client_pipe, &output, sizeof(int)) < 0 || output < 0) { //posso ler diretamente para um inteiro????? e ver logo o output???
        return -1;
    }

    close(rx_client_pipe);
    close(tx_server_pipe);
    
    if (unlink(c_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }
    printf("Successful unmount.\n");
    return 0;
}

int tfs_open(char const *name, int flags) {
    
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
