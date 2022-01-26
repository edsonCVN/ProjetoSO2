#include "operations.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

char sessions_table[S][PIPE_PATH_SIZE];
allocation_state_t free_sessions[S];
int opened_sessions;
int rx_server_pipe;

int server_init(char const *pipename) {
    
    if (unlink(pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipename, strerror(errno));
        return -1;
    }

    // creates clients->sever pipe
    if (mkfifo(pipename, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        return -1;
    }

    // opens clients->server pipe for reading
    rx_server_pipe = open(pipename, O_RDONLY);
    if (rx_server_pipe == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        return -1;
    }

    for(int i = 0; i < S; i++) {
        free_sessions[i] = FREE;
    }

    opened_sessions = 0;

    return 0;
}
/*  Receives the new client's pipe path name.
    If successful retuns the new session_id, else return -1.
*/
int add_new_session (char const *client_pipe_path) {
    
    for (int i = 0; i < S; i++) {
        if (free_sessions[i] == FREE) {
            free_sessions[i] = TAKEN;
            memcpy(sessions_table[i], client_pipe_path, PIPE_PATH_SIZE);
            opened_sessions++;
            return i;
        }
    }
    return -1;
}

/*  Receives the new client's pipe path and creates a new session.
    In the process opens the client's pipe to comunicate the new session_id if 
    successful (else -1). If mount is successful returns 0 else -1. 
*/
int tfs_server_mount(char const* client_pipe_path) {
    int tx = open(client_pipe_path, O_WRONLY);
    if (tx == -1) {
        return -1;
    }

    //criar nova sessão
    if (opened_sessions == S) {
        if(write(tx, -1, sizeof(int)) < 0){
            close(tx);
            return -1;
        }
        close(tx);
    }
            
    int session_id = add_new_session(client_pipe_path);
    if(session_id < 0) {
        if(write(tx, -1, sizeof(int)) < 0){
            close(tx);
            return -1;
        }
        close(tx);
    }
            
    if(write(tx, session_id, sizeof(int)) < 0) {
        close(tx);
        return -1;
    }
    return 0;
    close(tx);
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    if(server_init(pipename) < 0) {
        exit(EXIT_FAILURE);
    }

    while (true) { //com open e close do pipe clients->server ?
        char buffer[SERVER_BUFFER_SIZE];
        int op_code;
        
        ssize_t ret = read(rx_server_pipe, buffer, SERVER_BUFFER_SIZE - 1);
        if (ret == 0) {
            // ret == 0 signals EOF;
            return 0; //not sure ???????????????????????
        } else if (ret == -1) {
            // ret == -1 signals error
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        buffer[ret] = 0; //mesmo necessário (sendo que o buffer é previamente inicializado a '\0'?)
    
        op_code = atoi(strtok(buffer, "|"));

        switch (op_code) {
        case TFS_OP_CODE_MOUNT:

            char *client_pipe_path = atoi(strtok(NULL, "|"));
            if(tfs_server_mount(client_pipe_path) < 0) {
                exit(EXIT_FAILURE);
            }
            break;
        
        case TFS_OP_CODE_UNMOUNT:
            /* code */
            break;
        
        case TFS_OP_CODE_OPEN:
            /* code */
            break;
        
        case TFS_OP_CODE_CLOSE:
            /* code */
            break;

        case TFS_OP_CODE_WRITE:
            /* code */
            break;
        
        case TFS_OP_CODE_READ:
            /* code */
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            /* code */
            break;
        
        default:
            break;
        }
        
        

    }
     

    return 0;
}