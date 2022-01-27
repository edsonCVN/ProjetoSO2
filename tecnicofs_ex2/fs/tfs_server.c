#include "operations.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

int sessions_tx_table[S];
allocation_state_t free_sessions[S];
int opened_sessions;
int rx_server_pipe;

int return_value;


int server_init(char const *pipename) {
    
    if (unlink(pipename) != 0 && errno != ENOENT) {
        return -1;
    }

    // creates clients->sever pipe
    if (mkfifo(pipename, 0640) != 0) {
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
int add_new_session (int client_pipe_tx) {
    
    for (int i = 0; i < S; i++) {
        if (free_sessions[i] == FREE) {
            free_sessions[i] = TAKEN;
            sessions_tx_table[i] = client_pipe_tx;
            opened_sessions++;
            return i;
        }
    }
    return -1;
}

int delete_session (int session_id) {
    if (free_sessions[session_id] == TAKEN) {
        free_sessions[session_id] = FREE;
        sessions_tx_table[session_id] = -1;
        opened_sessions--;
        return 0;
    }
    return -1;
}

/*  Receives the new client's pipe path and creates a new session.
    In the process opens the client's pipe to comunicate the new session_id if 
    successful (else -1). If mount is successful returns 0 else -1. 
*/
void tfs_server_mount(char const* client_pipe_path) {
    int tx = open(client_pipe_path, O_WRONLY);
    if (tx == -1) {
        return;
    }

    //criar nova sessão
    if (opened_sessions == S) {
        return_value = -1;
        //write(tx, -1, sizeof(int));
        write(tx, &return_value, sizeof(int));
        close(tx);
        return;
    }
            
    int session_id = add_new_session(tx);
    if(session_id < 0) {
        write(tx, -1, sizeof(int));
        close(tx);
        return;
    }
            
    if(write(tx, session_id, sizeof(int)) < 0) { //não trata de situação onde não consegue escrever
        close(tx);
        return;
    }
}

void tfs_server_unmount(int session_id) {
    int tx = sessions_tx_table[session_id];
    delete_session(session_id);
    write(tx, delete_session(session_id), sizeof(int)); //não trata de situação onde não consegue escrever
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
        
        // opens clients->server pipe for reading
        rx_server_pipe = open(pipename, O_RDONLY);
        if (rx_server_pipe == -1) {
            return -1;
        }

        ssize_t ret = read(rx_server_pipe, buffer, SERVER_BUFFER_SIZE - 1);
        if (ret == 0) {
            close(rx_server_pipe);
            continue;
        } else if (ret == -1) {
            // ret == -1 signals error
            close(rx_server_pipe);
            exit(EXIT_FAILURE);
        }
        buffer[ret] = 0; //mesmo necessário (sendo que o buffer é previamente inicializado a '\0'?)
    
        op_code = atoi(strtok(buffer, "|"));
        
        int session_id;
        char *client_pipe_path = NULL;
        
        switch (op_code) {

        case TFS_OP_CODE_MOUNT:
            
            client_pipe_path = strtok(NULL, "|");
            tfs_server_mount(client_pipe_path);
            //tfs_server_mount(strtok(NULL, "|"));

            break;
        
        case TFS_OP_CODE_UNMOUNT:
            session_id = atoi(strtok(NULL, "|"));
            tfs_server_unmount(session_id);
            //tfs_server_unmount(atoi(strtok(NULL, "|")));
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
            //unlink do server_pipe
            break;
        
        default:
            break;
        }
        close(rx_server_pipe);
    }
     
    //close do pipe do servidor no do destroy?
    return 0;
}