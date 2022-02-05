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

typedef struct {
    int session_id;
    int session_tx;
    allocation_state_t session_state;
    pthread_t session_tid;
    bool buffer_on;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
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

    if (unlink(pipename) != 0 && errno != ENOENT) {
        return -1;
    }

    if (mkfifo(pipename, 0640) != 0) {
        return -1;
    }

    for(int i = 0; i < S; i++) {
        sessions_table[i].session_id = i;
        sessions_table[i].session_tx = -1;
        sessions_table[i].session_state = FREE;
        sessions_table[i].buffer_on = false;
        
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


ssize_t server_write_pipe(int fd, void *buf, size_t n, int session_id) {
    ssize_t written = 0;
    int tx = -1;
    while(written < n) {
        written += write(fd, buf + written, n - (size_t)written);
        if(errno == EINTR) {
            continue;
        } else if(errno == EPIPE) {
            if(pthread_mutex_lock(&mutex) < 0) {
                printf("[ERROR]\n");
                exit(EXIT_FAILURE);
            }
            if (session_id != NO_SESSION) {
                tx = sessions_table[session_id].session_tx;
                delete_session(session_id);
                close(tx);
            }
            return -1;
        }
    }
    return written;
}

ssize_t server_read_pipe(int fd, void *buf, size_t nbytes) {
    ssize_t already_read = 0;

    while(already_read < nbytes) {
        already_read += read(fd, buf + already_read, nbytes - (size_t)already_read);
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

    if(server_read_pipe(rx_server_pipe, client_pipe_path, PATH_SIZE) < 0) {
        return;
    }
    
    int tx = open(client_pipe_path, O_WRONLY);
    if (tx == -1) {
        return;
    }

    if(pthread_mutex_lock(&mutex) < 0) {
        server_write_pipe(tx, &return_value, sizeof(int), NO_SESSION);
        close(tx);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    
    if (opened_sessions == S) {
        if(pthread_mutex_unlock(&mutex) <0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        return_value = -1;
        server_write_pipe(tx, &return_value, sizeof(int), NO_SESSION);
        close(tx);
        return;
    }
            
    return_value = add_new_session(tx);

    if(return_value < 0) {
        if(pthread_mutex_unlock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        return_value = -1;
        server_write_pipe(tx, &return_value, sizeof(int), NO_SESSION);
        close(tx);
        return;
    }

    if(server_write_pipe(tx, &return_value, sizeof(int), return_value) < 0) {
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
    
    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(pthread_mutex_lock(&mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);        
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        pthread_mutex_unlock(&mutex);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    buffer[session_id].op_code = TFS_OP_CODE_UNMOUNT;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);
    
    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_open() {
    int session_id;
    char filename[PATH_SIZE];
    int flags;
    int return_value = -1;

    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(server_read_pipe(rx_server_pipe, filename, PATH_SIZE) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }
    if(server_read_pipe(rx_server_pipe, &flags, sizeof(int)) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

    memcpy(buffer[session_id].filename, filename, PATH_SIZE);
    buffer[session_id].flags = flags;
    buffer[session_id].op_code = TFS_OP_CODE_OPEN;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);
    
   if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_close() {
    
    int session_id;
    int fhandle;
    int return_value = -1;

    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(server_read_pipe(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

    buffer[session_id].fhandle = fhandle;
    buffer[session_id].op_code = TFS_OP_CODE_CLOSE;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);

    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_write() {
    int session_id; 
    int fhandle;
    size_t len; 
    int return_value = -1;
    
    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

    if(server_read_pipe(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(server_read_pipe(rx_server_pipe, &len, sizeof(size_t)) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
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
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;

    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    
    buffer[session_id].fhandle = fhandle;
    buffer[session_id].len = len;
    buffer[session_id].buf = buff;
    
    buffer[session_id].op_code = TFS_OP_CODE_WRITE;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);
    
    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
}

void tfs_server_read() {
    int session_id;
    int fhandle;
    size_t len; 
    int read_len = -1;

   if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        return;
    }

   if(server_read_pipe(rx_server_pipe, &fhandle, sizeof(int)) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &read_len, sizeof(int), session_id);
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    if(server_read_pipe(rx_server_pipe, &len, sizeof(size_t)) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &read_len, sizeof(int), session_id);
        if(pthread_mutex_lock(&mutex) < 0) {
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        delete_session(session_id);
        return;
    }

    void *buff = malloc(sizeof(char)* len);
    
    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &read_len, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
    
    buffer[session_id].fhandle = fhandle;
    buffer[session_id].len = len;
    buffer[session_id].buf = buff;

    buffer[session_id].op_code = TFS_OP_CODE_READ;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);

    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &read_len, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }
   
}

void tfs_shutdown_after_all_closed() {
    int session_id; 
    int return_value = -1;

    if(server_read_pipe(rx_server_pipe, &session_id, sizeof(int)) < 0) {
        
        return;
    }

    if(pthread_mutex_lock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

    buffer[session_id].op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    sessions_table[session_id].buffer_on = true;
    pthread_cond_signal(&sessions_table[session_id].cond);

    if(pthread_mutex_unlock(&sessions_table[session_id].mutex) < 0) {
        server_write_pipe(sessions_table[session_id].session_tx, &return_value, sizeof(int), session_id);
        printf("[ERROR]\n");
        exit(EXIT_FAILURE);
    }

}

void *worker_thread(void* session_id) {

    int tx = -1;

    while(true) {
        int id = *((int*)session_id); 
        int return_value = -1;

        if(pthread_mutex_lock(&sessions_table[id].mutex) < 0) {
            server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id);
            printf("[ERROR]\n");
            exit(EXIT_FAILURE);
        }
        while (!sessions_table[id].buffer_on) {
            pthread_cond_wait(&sessions_table[id].cond, &sessions_table[id].mutex);
        }

        switch (buffer[id].op_code) {

            case TFS_OP_CODE_UNMOUNT:
                
                tx = sessions_table[id].session_tx;
                return_value = delete_session(id);
                server_write_pipe(tx, &return_value, sizeof(int), id);
                close(tx);
                break;
            
            case TFS_OP_CODE_OPEN:
                return_value = tfs_open(buffer[id].filename, buffer[id].flags);
                server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id);
                break;
            
            case TFS_OP_CODE_CLOSE:
                return_value = tfs_close(buffer[id].fhandle);
                server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id);
                break;

            case TFS_OP_CODE_WRITE:
                return_value = (int) tfs_write(buffer[id].fhandle, buffer[id].buf, buffer[id].len);
                server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id);
                free(buffer[id].buf);
                break;
            
            case TFS_OP_CODE_READ:  
                
                return_value = (int) tfs_read(buffer[id].fhandle, buffer[id].buf, buffer[id].len);
              
                if(server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id) < 0) {
                    return_value = -1;
                    server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id);
                }
                if(return_value > 0) { 
                    server_write_pipe(sessions_table[id].session_tx, buffer[id].buf, (size_t) return_value, id);
                }
                free(buffer[id].buf);
                break;

            case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED: 
                return_value = tfs_destroy_after_all_closed();
                server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id);
    
                if(return_value == 0) {
                    pthread_mutex_unlock(&sessions_table[id].mutex);
                    for (int i = 0; i < S; i++) {
                        if(sessions_table[i].session_state == TAKEN) {
                            delete_session(i);
                            close(sessions_table[i].session_tx);
                            
                        }
                        pthread_mutex_destroy(&sessions_table[i].mutex);
                        pthread_cond_destroy(&sessions_table[i].cond);
                    }
                    pthread_mutex_destroy(&mutex);
                    
                    close(rx_server_pipe);
                    if (unlink(pipename) != 0 && errno != ENOENT) {
                       exit(EXIT_FAILURE);
                    }

                    printf("Goodbye\n");
                    exit(EXIT_SUCCESS);
                }
                break;

            default:
                break;
            
        }
        sessions_table[id].buffer_on = false;
        if(pthread_mutex_unlock(&sessions_table[id].mutex) < 0) {
            return_value = -1;
            server_write_pipe(sessions_table[id].session_tx, &return_value, sizeof(int), id);
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
        exit(EXIT_FAILURE);
    }

    apanhaSIGPIPE();
    while (true) {
        
        char op_code;

        ssize_t ret = read(rx_server_pipe, &op_code, sizeof(char));
        
        if (ret == 0) {
            close(rx_server_pipe);
            rx_server_pipe = open(pipename, O_RDONLY);
            if (rx_server_pipe == -1) {
                return -1;
            }
            continue;
        } else if (ret == -1) {
            close(rx_server_pipe);
            exit(EXIT_FAILURE);
        }

        switch (op_code) {

        case TFS_OP_CODE_MOUNT:
            printf("ggg\n");
            tfs_server_mount();
           
            break;

        case TFS_OP_CODE_UNMOUNT:
            
            tfs_server_unmount();
            
            break;
        
        case TFS_OP_CODE_OPEN:
        
            tfs_server_open();

            break;
        
        case TFS_OP_CODE_CLOSE:
         
            tfs_server_close();

            break;

        case TFS_OP_CODE_WRITE:
            
            tfs_server_write();
            
            break;
        
        case TFS_OP_CODE_READ:

            tfs_server_read();

            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:

            tfs_shutdown_after_all_closed();

            break;
        
        default:
            break;
        }

        
    }
     
    return 0;
}