#include "tecnicofs_client_api.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

//FIX ME como é o que o cliente guarda o session_id?

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    
    char intput[MOUNT_BUFFER_SIZE];
    int output;

    //buffer's initialization
    memset(intput, '\0', MOUNT_BUFFER_SIZE);
    memcpy(intput, TFS_OP_CODE_MOUNT + "|", 2);
    memcpy(intput + 2, client_pipe_path, strlen(client_pipe_path));

    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }

    // create pipe
    if (mkfifo(client_pipe_path, 0640) != 0) {
        return -1;
    }

    // first open clients->server pipe for write 
    int tx = open(server_pipe_path, O_WRONLY);
    if (tx == -1) {
        return -1;
    }
    
    if(write(tx, intput, MOUNT_BUFFER_SIZE) < 0) {
        return -1;
    }
    
    //assumindo que já foi aberto para escrita no server
    int rx = open(client_pipe_path, O_RDONLY);
    if(rx == -1) {
        return -1;
    }
    
    if(read(rx, output, sizeof(int)) < 0 || output < 0) { //posso ler diretamente para um inteiro????? e ver logo o output???
        return -1;
    }

    /* FIX ME: guardar o session id */

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
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
