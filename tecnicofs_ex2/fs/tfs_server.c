#include "operations.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
//#include <common.h> ??

/* VERIFICAR TODAS AS CONDIÇÕES DE ERRO, LOCKS, UNLOCKS, INITS...*/
typedef struct {
    int session_id;
    int session_tx;
    allocation_state_t session_state;
    pthread_t session_tid;
    bool buffer_on;
    pthread_cond_t cond;
    pthread_mutex_t mutex; //falta inicializar
} Session;

typedef struct {
    char op_code;
    char filename[PATH_SIZE];
    int flags;
    int fhandle;
    size_t len; 
    void * buf;
} Request;

Request buffer[S];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int opened_sessions;
int rx_server_pipe;
Session sessions_table[S];
char *pipename;

void *worker_thread(void* session_id);

void apanhaSIGPIPE () {
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

int server_init() {
    
    /*
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR){
        printf("[ERROR]\n");
        return -1;
    }
    SIG_IGN - sig ignore
    */
    
    if (unlink(pipename) != 0 && errno != ENOENT) {
        return -1;
    }

    // creates clients->sever pipe
    if (mkfifo(pipename, 0640) != 0) {
        return -1;
    }

   
    for(int i = 0; i < S; i++) {
        sessions_table[i].session_id = i;
        sessions_table[i].session_tx = -1;
        sessions_table[i].session_state = FREE;
        sessions_table[i].buffer_on = false;
        /*if(pthread_init(sessions_table[i].session_tid, NULL) < 0) {
            return -1;
        }*/
        if(pthread_mutex_init(&sessions_table[i].mutex, NULL) < 0) {
            return -1;
        }
        if(pthread_cond_init(&sessions_table[i].cond, NULL) < 0) {
            return -1;
        }
        if (pthread_create(&sessions_table[i].session_tid, NULL, worker_thread, (void *) &sessions_table[i].session_id) < 0) {
            return -1;
        }
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
        if (sessions_table[i].session_state == FREE) {
            sessions_table[i].session_state = TAKEN;
            sessions_table[i].session_tx = client_pipe_tx;
            opened_sessions++;
            return i;
        }
    }
    return -1;
}

int delete_session (int session_id) {
    
    if (sessions_table[session_id].session_state == TAKEN) {
        sessions_table[session_id].session_state = FREE;
        sessions_table[session_id].session_tx = -1;
        opened_sessions--;
        if(pthread_mutex_unlock(&mutex) <0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        return 0;
    }
    if(pthread_mutex_unlock(&mutex) <0) {
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    return -1;
    
}

/*  Receives the new client's pipe path and creates a new session.
    In the process opens the client's pipe to comunicate the new session_id if 
    successful (else -1). If mount is successful returns 0 else -1. 
*/


int server_write_pipe(int fd, void *buf, size_t n, int session_id) {
    size_t written = 0;
    
    while(written < n) { //inc?
        written += write(fd, buf + written, n - written);
        if(errno == EINTR) {
            continue;
        } else if(errno = EPIPE) {
            if(pthread_mutex_lock(&mutex) < 0) {
                printf("[ERROR]\n");
                exit(EXIT_FAILURE);
            }
            delete_session(session_id);
            return -1;
        }
    }
    return written;
}

int server_read_pipe(int fd, void *buf, size_t nbytes) {
    size_t already_read = 0;

    while(already_read < nbytes) { //inc?
        already_read += read(fd, buf + already_read, nbytes - already_read);
        if(errno == EINTR) {
            continue;
        } else if(errno == EBADF) {
            rx_server_pipe = open(pipename, O_RDONLY);
            if (rx_server_pipe == -1) {
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


void tfs_server_mount() {

    int return_value = -1;
    char client_pipe_path[PATH_SIZE];

    /*
    ssize_t ret = read(rx_server_pipe, client_pipe_path, PATH_SIZE);
    if (ret == 0) {
        close(rx_server_pipe);
        return ;
    } else if (ret == -1) {
        // ret == -1 signals error //signal!
        close(rx_server_pipe);
        return ; //completar
    }*/

    if(server_read_pipe(rx_server_pipe, client_pipe_path, PATH_SIZE) < 0) {
        return;
    }
    
    int tx = open(client_pipe_path, O_WRONLY);
    if (tx == -1) {
        return;
    }

    if(pthread_mutex_lock(&mutex) < 0) {
        write(tx, &return_value, sizeof(int));
        close(tx);
        return;
    }
    
    if (opened_sessions == S) {
        if(pthread_mutex_unlock(&mutex) <0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        return_value = -1;
        write(tx, &return_value, sizeof(int));
        close(tx);
        return;
    }
            
    return_value = add_new_session(tx);

    if(return_value < 0) {
        if(pthread_mutex_unlock(&mutex) <0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        return_value = -1;
        write(tx, &return_value, sizeof(int));
        close(tx);
        return;
    }

    if(write(tx, &return_value, sizeof(int)) < 0) { //retry
        if(pthread_mutex_unlock(&mutex) <0) {
            printf("[ERROR]\n");
        }
        close(tx);
        return;
    }

    if(pthread_mutex_unlock(&mutex) < 0) {
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_unmount() {

    int return_value = -1;
    int session_id;
    
    /* subst. read
    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }
    */
    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(pthread_mutex_lock(&mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        pthread_mutex_unlock(&mutex);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    buffer[session_id].op_code = TFS_OP_CODE_UNMOUNT;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);
    
    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    /* global mutex unlock inside delete_session */   
}

void tfs_server_open() {
    int session_id;
    char filename[PATH_SIZE];
    int flags;
    int fhandle;
    //passar para buffer
    int return_value = -1;

    /*
    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(read(rx_server_pipe, buffer[session_id].filename, PATH_SIZE) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        return;
    }
    if(read(rx_server_pipe, &buffer[session_id].flags, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        return;
    }
    */
    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(server_read_pipe(rx_server_pipe, filename, PATH_SIZE) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }
    if(server_read_pipe(rx_server_pipe, &flags, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

    memcpy(buffer[session_id].filename, filename, PATH_SIZE);
    buffer[session_id].flags = flags;
    buffer[session_id].op_code = TFS_OP_CODE_OPEN;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond); //signal? 
    
   if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_close() {
    
    int session_id;
    int fhandle;
    int return_value = -1;
    /*
    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(read(rx_server_pipe, &buffer[session_id].fhandle, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        return;
    }
    */

    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(server_read_pipe(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

    buffer[session_id].fhandle = fhandle;
    buffer[session_id].op_code = TFS_OP_CODE_CLOSE;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);

    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_write() {
    int session_id; 
    int fhandle;
    size_t len; 
    int return_value = -1;

    /*
    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }
    
    if(read(rx_server_pipe, &buffer[session_id].fhandle, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        return;
    }

    if(read(rx_server_pipe, &buffer[session_id].len, sizeof(size_t)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        return;
    }
    */
    
    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(server_read_pipe(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(server_read_pipe(rx_server_pipe, &len, sizeof(size_t)) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }
    

    void *buff = malloc(sizeof(char)* len);


    if(server_read_pipe(rx_server_pipe, buff, len) < 0) {
        free(buff);
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;

    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    
    buffer[session_id].fhandle = fhandle;
    buffer[session_id].len = len;
    buffer[session_id].buf = buff;
    
    buffer[session_id].op_code = TFS_OP_CODE_WRITE;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond); //signal? 
    
    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &return_value, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_read() {
    int session_id;
    int fhandle;
    size_t len; 
    int read_len = -1;

    /*
    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }
    
    if(read(rx_server_pipe, &buffer[session_id].fhandle, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &read_len, sizeof(int));
        return;
    }

    if(read(rx_server_pipe, &buffer[session_id].len, sizeof(size_t)) < 0) {
        write(sessions_table[session_id].session_tx, &read_len, sizeof(int));
        return;
    }
    */

   if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

   if(server_read_pipe(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        write(sessions_table[session_id].session_tx, &read_len, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(server_read_pipe(rx_server_pipe, &len, sizeof(size_t)) < 0) {
        write(sessions_table[session_id].session_tx, &read_len, sizeof(int));
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    void *buff = malloc(sizeof(char)* len);
    
    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &read_len, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    
    buffer[session_id].fhandle = fhandle;
    buffer[session_id].len = len;
    buffer[session_id].buf = buff;

    buffer[session_id].op_code = TFS_OP_CODE_READ;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond); //signal? 

    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        write(sessions_table[session_id].session_tx, &read_len, sizeof(int)); //não trata de situação onde não consegue escrever
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

    

    /*

    char buffer[len];
    read_len = tfs_read(fhandle, (void*) buffer, len);

    if(write(sessions_tx_table[session_id], &read_len, sizeof(int)) < 0) {
        read_len = -1;
        write(sessions_tx_table[session_id], &read_len, sizeof(int));
        return;
    }

    if(read_len > 0) { 
        write(sessions_tx_table[session_id], buffer, read_len);
    }
    
    //write(sessions_tx_table[session_id], &read_len, sizeof(int));*/
}

void tfs_shutdown_after_all_closed() {
    int session_id; 

    if(read(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        
        return;
    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

    buffer[session_id].op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond); //signal? 

    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

}

void *worker_thread(void* session_id) {

    int tx = -1;

    while(true) {
        int id = *((int*)session_id); //testar
        int return_value = -1;

        if(pthread_mutex_lock(&sessions_table[id].mutex) < 0) {
            write(sessions_table[id].session_tx, &return_value, sizeof(int)); //POR FAZER TRATAMENTO DE ERROS
            continue;
        }
        while (!sessions_table[id].buffer_on) {
            pthread_cond_wait(&sessions_table[id].cond, &sessions_table->mutex);
        }
        //unlock?

        switch (buffer[id].op_code) {

            case TFS_OP_CODE_UNMOUNT:
                
                tx = sessions_table[id].session_tx;
                return_value = delete_session(id);
                write(tx, &return_value, sizeof(int)); //signal
                close(sessions_table[id].session_tx);
                break;
            
            case TFS_OP_CODE_OPEN:
                return_value = tfs_open(buffer[id].filename, buffer[id].flags);
                write(sessions_table[id].session_tx, &return_value, sizeof(int)); //signal
                break;
            
            case TFS_OP_CODE_CLOSE:
                return_value = tfs_close(buffer[id].fhandle);
                write(sessions_table[id].session_tx, &return_value, sizeof(int)); //signal
                break;

            case TFS_OP_CODE_WRITE:
                return_value = tfs_write(buffer[id].fhandle, buffer[id].buf, buffer[id].len);
                write(sessions_table[id].session_tx, &return_value, sizeof(int)); //signal
                printf("write sig error %s\n", strerror(errno));
                free(buffer[id].buf);
                buffer[id].buf = NULL;
                break;
            
            case TFS_OP_CODE_READ:  
                return_value = tfs_read(buffer[id].fhandle, buffer[id].buf, buffer[id].len);
                if(write(sessions_table[id].session_tx, &return_value, sizeof(int)) < 0) { //signal
                    return_value = -1;
                    write(sessions_table[id].session_tx, &return_value, sizeof(int)); //signal
                }
                if(return_value > 0) { 
                    write(sessions_table[id].session_tx, buffer[id].buf, return_value); //signal
                }
                free(buffer[id].buf);
                buffer[id].buf = NULL;
                break;

            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED: //isto não devia ser feito pela tarefa recetora? (assumindo que era o shutdown completo do servidor)
                return_value = tfs_destroy_after_all_closed();
                write(sessions_table[id].session_tx, &return_value, sizeof(int)); //signal
                for (int i = 0; i < S; i++) {
                    if(sessions_table[i].session_state == TAKEN) {
                        /*return_value =*/ delete_session(i);
                        close(sessions_table[i].session_tx);
                        
                    }
                    pthread_mutex_destroy(&sessions_table[i].mutex);
                    pthread_cond_destroy(&sessions_table[i].cond);
                }
                close(rx_server_pipe);
                if (unlink(pipename) != 0 && errno != ENOENT) {
                    return 1;
                }


                //DUVIDA E OWRITE FINAL DO WHILE?

                break;
            
            default:
                break;
            
        }
        sessions_table[id].buffer_on = false;
        if(pthread_mutex_unlock(&sessions_table[id].mutex) < 0) {
            return_value = -1;
            write(sessions_table[id].session_tx, &return_value, sizeof(int)); //POR FAZER TRATAMENTO DE ERROS
        }
    }
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename_main = argv[1];
    pipename = pipename_main;
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    if(server_init() < 0) {
        exit(EXIT_FAILURE);//tratar
    }

    apanhaSIGPIPE();
    while (true) { //com open e close do pipe clients->server ?
        
        char op_code;

        ssize_t ret = read(rx_server_pipe, &op_code, sizeof(char));
        
        if (ret == 0) {
            // nothing to read
            close(rx_server_pipe);
            rx_server_pipe = open(pipename, O_RDONLY);
            if (rx_server_pipe == -1) {
                return -1;
            }
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
            printf("\t\tCURRENT OP_CODE:%d\n", op_code);
            printf("entrou SHUTDOWN\n");
            tfs_shutdown_after_all_closed();
            printf("saiu SHUTDOWN\n");
            /* code */
            //unlink do server_pipe
            //e tfs_destroy
            //close(rx_server_pipe);
            //destroy thread, mutex, cond
            break;
        
        default:
            break;
        }

        
    }
     
    //close do pipe do servidor no do destroy?
    return 0;
}