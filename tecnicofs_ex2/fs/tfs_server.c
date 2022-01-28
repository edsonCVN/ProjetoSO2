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
void tfs_server_mount() {

    //char buffer[PIPE_PATH_SIZE];
    int return_value = -1;
    char client_pipe_path[PIPE_PATH_SIZE];

    ssize_t ret = read(rx_server_pipe, client_pipe_path, PIPE_PATH_SIZE);
    if (ret == 0) {
        close(rx_server_pipe);
        return ;
    } else if (ret == -1) {
        // ret == -1 signals error
        close(rx_server_pipe);
        return ; //completar
    }
  
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
            
    //int session_id = add_new_session(tx);
    return_value = add_new_session(tx);

    if(return_value < 0) {
        write(tx, &return_value, sizeof(int));
        close(tx);
        return;
    }

    //problemas de sincornização, recorrer a return_value?
    if(write(tx, &return_value, sizeof(int)) < 0) { //não trata de situação onde não consegue escrever
        close(tx);
        return;
    }
}

void tfs_server_unmount() {

    int return_value = -1;
    char buffer[1];
    int session_id;
    
    ssize_t ret = read(rx_server_pipe, buffer, 1);
    if (ret == 0) {
        close(rx_server_pipe);
        return;
    } else if (ret == -1) {
        // ret == -1 signals error
        close(rx_server_pipe);
        return ; //completar
    }

    session_id = atoi(buffer);

    int tx = sessions_tx_table[session_id];
    return_value = delete_session(session_id);
    write(tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
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
        char buffer[2];
        // opens clients->server pipe for reading
        rx_server_pipe = open(pipename, O_RDONLY);
        if (rx_server_pipe == -1) {
            return -1;
        }

        ssize_t ret = read(rx_server_pipe, buffer, 1);
        
        buffer[ret] = 0;
        
        if (ret == 0) {
           
            close(rx_server_pipe);
            continue;
        } else if (ret == -1) {
            // ret == -1 signals error
            close(rx_server_pipe);
            exit(EXIT_FAILURE);
        }
        buffer[ret] = 0;
        int op_code = atoi(buffer);
        
        switch (op_code) {

        case TFS_OP_CODE_MOUNT:

            printf("entrou mount\n");
            tfs_server_mount();
            printf("saiu mount\n");
            break;
        
        case TFS_OP_CODE_UNMOUNT:
            printf("entrou unmount\n");
            tfs_server_unmount();
            printf("saiu unmount\n");
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