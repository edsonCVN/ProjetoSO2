#include "tecnicofs_client_api.h"


int session_id = -1;
int tx_server_pipe = -1;
int rx_client_pipe = -1;
char c_pipe_path[PATH_SIZE];

ssize_t client_read_pipe(int fd, void *buf, size_t nbytes) {
    ssize_t already_read = 0;

    while(already_read < nbytes) {
        already_read += read(fd, buf + already_read, nbytes - (size_t)already_read);
        if(errno == EINTR) {
            continue;
        } else if(errno == EBADF) {
            rx_client_pipe = open(c_pipe_path, O_RDONLY);
            if (rx_client_pipe == -1) {
                printf("[ERROR]\n");
                exit(EXIT_FAILURE);
            }
            continue;
        } else if (errno != 0 && already_read != nbytes) {
            return -1;
        }
    }
    
    return already_read;
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    
    char code = TFS_OP_CODE_MOUNT;
    char input[1 + PATH_SIZE];
    int output = -1;

    memset(input, '\0', 1 + PATH_SIZE);
    input[0] = code;
    memcpy(input + 1, client_pipe_path, strlen(client_pipe_path));
    
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }

    if (mkfifo(client_pipe_path, 0640) != 0) {
        return -1;
    }

    tx_server_pipe = open(server_pipe_path, O_WRONLY);
    if (tx_server_pipe == -1) {
        return -1;
    }
    
    if (write(tx_server_pipe, input, 1 + PATH_SIZE) < 0) {
        return -1;
    }

    rx_client_pipe = open(client_pipe_path, O_RDONLY);
    if(rx_client_pipe == -1) {
        return -1;
    }
    
    if(client_read_pipe(rx_client_pipe, &output, sizeof(int)) < 0 || output < 0) {
        return -1;
    }

    session_id = output;
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
    
    if(client_read_pipe(rx_client_pipe, &output, sizeof(int)) < 0 || output < 0) { 
        return -1;
    }
    
    close(tx_server_pipe);
    close(rx_client_pipe);
    
    if (unlink(c_pipe_path) != 0 && errno != ENOENT) {
        return -1;
    }
   
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

    if(client_read_pipe(rx_client_pipe, &output, sizeof(int)) < 0) { 
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
    
    if(client_read_pipe(rx_client_pipe, &output, sizeof(int)) < 0) { 
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

    if(client_read_pipe(rx_client_pipe, &output, sizeof(int)) < 0) { 
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

    if(client_read_pipe(rx_client_pipe, &read_len, sizeof(int)) < 0) { 
        return -1;
    }

    if(!(read_len >= 0 && client_read_pipe(rx_client_pipe, buffer, (size_t)read_len) > 0)) { 
        return -1;
    }
    return (ssize_t)read_len;
}

int tfs_shutdown_after_all_closed() {
    char code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    char input[1 + sizeof(int)];
    int output = -1;

    input[0] = code;
    memcpy(input + 1, &session_id, sizeof(int));

    if(write(tx_server_pipe, &input, 1 + sizeof(int)) < 0) {
        return -1;
    }
    
    if(client_read_pipe(rx_client_pipe, &output, sizeof(int)) < 0) { 
        return -1;
    }
    return output;
}
