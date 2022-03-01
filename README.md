# ProjetoSO2

Projeto de Sistemas Operativos 2021-22
Enunciado do 2o exercício LEIC-A/LEIC-T/LETI

O 2o exercício do projeto de SO pretende estender o sistema de ficheiros TecnicoFS com a capacidade de servir vários processos cliente de forma concorrente.
Para tal, o TecnicoFS deixará de ser uma simples biblioteca para passar a ser um processo servidor autónomo, ao qual diferentes processos clientes se podem ligar e enviar mensagens com pedidos de operações.

Ponto de partida

Para resolver o 2o exercício, os grupos devem descarregar o novo código base (tecnicofs_ex2.zip), disponibilizado no site da disciplina.
Este código base estende a versão original do TecnicoFS das seguintes maneiras:
a) As operações principais do TecnicoFS estão sincronizadas usando um único trinco (mutex) global. Embora menos paralela que a solução pretendida para o 1o exercício, esta solução de sincronização é suficiente para implementar os novos
requisitos que compõem o 2o exercício.
b) São fornecidos 2 programas clientes que podem ser usados como testes iniciais
para cada requisito descrito de seguida.
c) Para o requisito 2 (ver abaixo), é fornecido um esqueleto para o programa do
servidor TecnicoFS (em fs/tfs_server.c) e um esqueleto da implementação da API cliente do TecnicoFS (na diretoria client).
