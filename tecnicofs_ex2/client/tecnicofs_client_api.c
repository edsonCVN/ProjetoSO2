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
char c_pipe_path[PATH_SIZE];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    
    char code = TFS_OP_CODE_MOUNT;
    char input[1 + PATH_SIZE];
    int output = -1;

    memset(input, '\0', 1 + PATH_SIZE);
    input[0] = code;
    memcpy(input + 1, client_pipe_path, strlen(client_pipe_path));
    
    printf("input: %s\n", input);
    
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
    
    if (write(tx_server_pipe, input, 1 + PATH_SIZE) < 0) {
        return -1;
    }

    rx_client_pipe = open(client_pipe_path, O_RDONLY);
    if(rx_client_pipe == -1) {
        return -1;
    }
    
    if(read(rx_client_pipe, &output, sizeof(int)) < 0 || output < 0) {
        return -1;
    }
    
    session_id = output;
    printf("recebeu session_id: %d\n", session_id);

    return 0;
}

int tfs_unmount() {
    
    char code = TFS_OP_CODE_UNMOUNT;
    char input[1 + sizeof(int)];
    int output = -1;

    input[0] = code;
    memcpy(input + 1, &session_id, sizeof(int));

    if(write(tx_server_pipe,input, 1 + sizeof(int)) < 0) {
        return -1;
    }

    if(read(rx_client_pipe, &output, sizeof(int)) < 0 || output < 0) { 
        return -1;
    }
    printf("output %d\n", output);
    
    close(tx_server_pipe);
    //sleep(1);
    close(rx_client_pipe);
    
    if (unlink(c_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }
    printf("Successful unmount.\n");
    return 0;
}

int tfs_open(char const *name, int flags) {
    
    char code = TFS_OP_CODE_OPEN;
    char input[1 + 2*sizeof(int) + PATH_SIZE];
    int output = -1;
    
    memset(input, '\0', 1 + sizeof(int) + PATH_SIZE);

    input[0] = code;
    memcpy(input + 1, &session_id, sizeof(int));
    memcpy(input + 1 + sizeof(int), name, strlen(name)); 
    memcpy(input + 1 + sizeof(int) + PATH_SIZE, &flags, sizeof(int));   

    if(write(tx_server_pipe, &input, 1 + 2*sizeof(int) + PATH_SIZE) < 0) {
        return -1;
    }

    if(read(rx_client_pipe, &output, sizeof(int)) < 0) { 
        return -1;
    }

    return output;
}

int tfs_close(int fhandle) {
    
    char code = TFS_OP_CODE_CLOSE;
    char input[1 + 2*sizeof(int)];
    int output = -1;

    input[0] = code;
    memcpy(input + 1, &session_id, sizeof(int));
    memcpy(input + 1 + sizeof(int), &fhandle, sizeof(int));

    if(write(tx_server_pipe, &input, 1 + 2*sizeof(int)) < 0) {
        return -1;
    }
    
    if(read(rx_client_pipe, &output, sizeof(int)) < 0) { 
        return -1;
    }
    return output;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    
    char code = TFS_OP_CODE_WRITE;
    char input[1 + 2*sizeof(int)+ sizeof(size_t) + len];
    int output = -1;
    
    input[0] = code;
    memcpy(input + 1, &session_id, sizeof(int));
    memcpy(input + 1 + sizeof(int), &fhandle, sizeof(int));
    memcpy(input + 1 + 2*sizeof(int), &len, sizeof(size_t));
    memcpy(input + 1 + 2*sizeof(int) + sizeof(size_t), buffer, len);

    if(write(tx_server_pipe, input, 1 + 2*sizeof(int)+ sizeof(size_t) + len) < 0) {
        return -1;
    }
    sleep(10);
    if(read(rx_client_pipe, &output, sizeof(int)) < 0) { 
        return -1;
    }

    return (ssize_t) output;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    
    char code = TFS_OP_CODE_READ;
    char input[1 + 2*sizeof(int) + sizeof(size_t)];
    int read_len = -1;

    input[0] = code;
    memcpy(input + 1, &session_id, sizeof(int));
    memcpy(input + 1 + sizeof(int), &fhandle, sizeof(int));
    memcpy(input + 1 + 2*sizeof(int), &len, sizeof(size_t));

    if(write(tx_server_pipe, input, 1 + 2*sizeof(int) + sizeof(size_t)) < 0) {
        return -1;
    }

    if(read(rx_client_pipe, &read_len, sizeof(int)) < 0) { 
        return -1;
    }

    if(!(read_len >= 0 && read(rx_client_pipe, buffer, (size_t)read_len) > 0)) { 
        return -1;
    }
    return (ssize_t)read_len;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
