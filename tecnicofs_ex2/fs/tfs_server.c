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
    tfs_init();

    rx_server_pipe = open(pipename, O_RDONLY);
    if (rx_server_pipe == -1) {
        return -1;
    }

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
    char client_pipe_path[PATH_SIZE];

    ssize_t ret = read(rx_server_pipe, client_pipe_path, PATH_SIZE);
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
    int session_id;
    
    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }

    int tx = sessions_tx_table[session_id];
    return_value = delete_session(session_id);
    printf("aqui\n");
    write(tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
    close(tx);
    
}

void tfs_server_open() {
    int session_id;
    char const filename[PATH_SIZE];
    int flags;
    int return_value = -1;

    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }
    if(read(rx_server_pipe, filename, PATH_SIZE) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }
    if(read(rx_server_pipe, &flags, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }
    return_value = tfs_open(filename, flags);
    write(sessions_tx_table[session_id], &return_value, sizeof(int));
}

void tfs_server_close() {
    
    int session_id;
    int fhandle;
    int return_value = -1;

    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }

    if(read(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }

    return_value = tfs_close(fhandle);
    write(sessions_tx_table[session_id], &return_value, sizeof(int));
}

void tfs_server_write() {
    int session_id; 
    int fhandle; 
    size_t len; 
    //void const *buffer;
    int return_value = -1;

    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }
    
    if(read(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }

    if(read(rx_server_pipe, &len, sizeof(size_t)) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }

    char buffer[len];
    if(read(rx_server_pipe, buffer, len) < 0) {
        write(sessions_tx_table[session_id], &return_value, sizeof(int));
        return;
    }

    return_value = tfs_write(fhandle, buffer, len);
    write(sessions_tx_table[session_id], &return_value, sizeof(int));
}

void tfs_server_read() {
    int session_id; 
    int fhandle; 
    size_t len; 
    int read_len = -1;


    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &read_len, sizeof(int));
        return;
    }
    
    if(read(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        write(sessions_tx_table[session_id], &read_len, sizeof(int));
        return;
    }

    if(read(rx_server_pipe, &len, sizeof(size_t)) < 0) {
        write(sessions_tx_table[session_id], &read_len, sizeof(int));
        return;
    }
    
    char buffer[len];
    read_len = tfs_read(fhandle, (void*) buffer, len);

    if(write(sessions_tx_table[session_id], &read_len, sizeof(int)) < 0) { 
        write(sessions_tx_table[session_id], &read_len, sizeof(int));
        return;
    }

    if(!(read_len > 0 && write(sessions_tx_table[session_id], buffer, read_len) > 0)) { 
        read_len = -1;
    }
    write(sessions_tx_table[session_id], &read_len, sizeof(int));
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
        
        char op_code;

        ssize_t ret = read(rx_server_pipe, &op_code, sizeof(char));
        
        if (ret == 0) {
            // nothing to read
            continue;
        } else if (ret == -1) {
            // ret == -1 signals error
            close(rx_server_pipe);
            exit(EXIT_FAILURE);
        }

        switch (op_code) {

        case TFS_OP_CODE_MOUNT:
            printf("\t\tCURRENT OP_CODE:%d\n", op_code);
            printf("entrou mount\n");
            tfs_server_mount();
            printf("saiu mount\n");
            break;
        
        case TFS_OP_CODE_UNMOUNT:
            printf("\t\tCURRENT OP_CODE:%d\n", op_code);
            printf("entrou unmount\n");
            tfs_server_unmount();
            printf("saiu unmount\n");
            break;
        
        case TFS_OP_CODE_OPEN:
            printf("\t\tCURRENT OP_CODE:%d\n", op_code);
            printf("entrou open\n");
            tfs_server_open();
            printf("saiu open\n");
            break;
        
        case TFS_OP_CODE_CLOSE:
            printf("\t\tCURRENT OP_CODE:%d\n", op_code);
            printf("entrou close\n");
            tfs_server_close();
            printf("saiu close\n");
            break;

        case TFS_OP_CODE_WRITE:
            printf("\t\tCURRENT OP_CODE:%d\n", op_code);
            printf("entrou write\n");
            tfs_server_write();
            printf("saiu write\n");
            break;
        
        case TFS_OP_CODE_READ:
            printf("\t\tCURRENT OP_CODE:%d\n", op_code);
            printf("entrou read\n");
            tfs_server_read();
            printf("saiu read\n");
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            /* code */
            //unlink do server_pipe
            //e tfs_destroy
            //close(rx_server_pipe);
            break;
        
        default:
            break;
        }

        
    }
     
    //close do pipe do servidor no do destroy?
    return 0;
}