Código base do projeto de Sistemas Operativos
====================================

LEIC-A/LEIC-T/LETI, DEI/IST/ULisboa 2021-22
---------------------------------------------------------------

Consultar o enunciado publicado no site da disciplina

CHANGE LOG:
============

**Versão 4**
-----------------

Corrigido um erro no teste da função tfs_destroy_after_all_closed.

**Versão 3**
-----------------

Várias extensões para suportar o desenvolvimento do 2º exercício (consultar o enunciado respetivo).

**Versão 2**
-----------------

1. Removido um bloco “if” obsoleto na tfs_read (na v1, linhas 162-165 do operations.c): “if (file->of_offset + to_read >= BLOCK_SIZE) {...}”. Além de desnecessário, a condição continha um bug que poderia fazer com que uma leitura a um bloco cheio não devolvesse o resultado esperado.

2. O ciclo de “insert_delay” na função inode_create (na v1, linha 93 do state.c na v1) foi retificado para passar a ter um número de iterações mais realista.
O original (v1) era “if ((inumber * (int) sizeof(allocation_state_t)) == 0)”, passou para “if ((inumber * (int) sizeof(allocation_state_t) % BLOCK_SIZE) == 0)”.

3. Removido um comentário obsoleto (na v1, linhas 159-160 do state.c).

-----------------
-----------------

um macro para cada buffer?????
posiveis erros do unmount


o que fiz de manhã:
    substitui o asprintf pelo sprintf pq não conseguia retirar o erro.
    estou a passar os inteiros por referencia nos reads e writes (usando v.global return_value em alguns casos )
    (void) write para parar de dar o warning (apesar de não saber se será a melhor soulução no futuro)

    para correres os teste:
    terminal_servidor(em tecnicofs_ex2/fs): ./tfs_server /tmp/srv
    terminal_cliente(em tecnicofs_ex2): ./tests/client_server_simple_test /tmp/clt /tmp/srv
    
    executando o gdb no server (gdb --args ./tfs_server /tmp/srv) conclui que a linha 91 so tfs_server não está a funcionar (fala com o stor para ver a melhor opção, talvez passar tudo em string pelos pipes...)





para escrever/ler, no servidor devemos usar buffer's de tamanho fixo ou do tamnho len dado?



implementar buffer produtor/consumidor (com p e c) 
implementar worker_thread


só precisamos de uma posição do buffer
é impossível o produtor bloquear-se (é imediatamente consumido)

já foi consumido(basta sobrescrever)

e os locks?

PÔR LOCKS NO MOUNT E UNMOUNT

SIGNAL OU BROADCAST

Evitem terminar o servidor caso seja detetado um problema na comunicação com os clientes.
Os clientes devem podem terminar abruptamente (p.e., ctrl+c) sem causar a terminação do servidor. Nestes casos, devem detetar o problema e simplesmente tornar as tarefas trabalhadoras novamente disponíveis para processar outros pedidos.
\#########

1- A solução mais robusta é detetar o problema lado servidor (a mensagem não tem o formato esperado) e "cancelar" a sessão com o cliente.
2- Ao escrever para um pipe que já não existe, podem e devem "cancelar" a sessão. Este tipo de erro pode acontecer de qualquer forma se o cliente terminar após o pedido ter sido inserido no buffer e antes de ser processado.

###########################

Caso as funções lado servidor sobre os ficheiros falhem (p.e., o ficheiro que o cliente tenta abrir não existe) devem devolver -1 pelo fifo aos clientes.
Caso o fifo seja fechado pelo cliente e a seguir o servidor tenta escrever no fifo, o servidor recebe um sigpipe, cuja rotina por omissão termina o processo. Podes evitar este problema ignorando o SIGPIPE (sys. call signal). 
Se uma operação sobre os pipes (por exemplo write) falhar com -1 podem usar errno para decidir, dependendo da causa do erro, se devem tentar outra vez (por exemplo, EINTR) ou desistir (por exemplo, EPIPE).
Claramente deves usar loops para implementar a lógica de "retry", e não aninhar as tentativas em blocos "IF"s como ilustras no teu post. 

Espero ter conseguido ajudar.