/******************************************************************************

Biblioteca de simulacao a eventos discretos, baseada em atores/mensagens.

Copyright (C) 2007, Carlos A. Maziero (http://www.ppgia.pucpr.br/~maziero)
   
Este programa é software livre; você pode redistribuí-lo e/ou modificá-lo sob
os termos da Licença Pública Geral GNU, conforme publicada pela Free Software
Foundation; tanto a versão 2 da Licença como qualquer versão mais nova.

Este programa é distribuído na expectativa de ser útil, mas SEM QUALQUER
GARANTIA; sem mesmo a garantia implícita de COMERCIALIZAÇÃO ou de ADEQUAÇÃO
A QUALQUER PROPÓSITO EM PARTICULAR. Consulte a Licença Pública Geral GNU
para obter mais detalhes. A Licença Pública Geral GNU está disponível em
http://www.gnu.org/licenses

******************************************************************************/

#ifndef __SIMPATICA__
#define __SIMPATICA__

#include <float.h>

#define SIMPATICA_VERSION "0.7, 06/out/2007"

// algumas constantes de uso geral
#define INFINITY DBL_MAX // double infinito (para time-outs, etc)

#ifndef NULL
#define NULL ((void *) 0)
#endif

//----------------------------------------------------------------------------

// alguns tipos inteiros sem sinal
typedef unsigned int       uint ;
typedef unsigned long int  ulong  ;
typedef unsigned short int ushort ;
typedef unsigned char      uchar2 ;

//----------------------------------------------------------------------------

// politicas das filas
#define QUEUE_FIFO   0   // First-in, First-out
#define QUEUE_LIFO   1   // Last-in, First-out (pilha)

//----------------------------------------------------------------------------

// Inicializa as estruturas necessarias para a simulacao.
// Deve ser chamada sempre no inicio do programa principal.
// Os parametros informam o numero maximo de tarefas e filas criadas.
void init_simulation (uint maxTasks, uint maxQueues) ;

// executa a simulacao ate que o relogio atinja maxTime unidades de tempo.
void run_simulation (double maxTime) ;

// encerra a ultima simulacao, limpando todas as estruturas alocadas.
// Esta chamada permite fazer varias simulacoes no mesmo "main".
void kill_simulation () ;

// informa o valor atual do relogio de simulacao
double time_now () ;

// ativa mensagens de tracing no intervalo [startTime, stopTime]
void trace_interval (double startTime, double stopTime) ;

//----------------------------------------------------------------------------

// cria uma tarefa, retornando um ID unico, positivo e sequencial.
// Cada tarefa eh identificada pelo seu ID unico no sistema.
// A funcao taskBody representa o corpo da tarefa, e recebe startArg
// como parametro de entrada. stackPages indica o tamanho (em páginas)
// da pilha a ser alocada para a tarefa (no mínimo uma página).
// Cada tarefa tem uma fila de mensagens de entrada, no inicio vazia.
uint task_create (void (*taskBody)(void *), void* startArg, ushort stackPages) ;

// destroi a tarefa indicada, liberando os recursos utilizados por ela.
// retorna 0 em sucesso ou -1 em caso de erro. Toda tarefa que encerra
// sua vida util deve ser destruida para liberar memoria.
void task_destroy (uint task_id) ;

// encerra a tarefa corrente, liberando seus recursos.
// Deve ser chamada no final do codigo de cada task.
void task_exit () ;

// A tarefa corrente vai dormir t unidades de tempo
void task_sleep (double t) ;

// A tarefa corrente vai dormir ate ser acordada por outra tarefa
// atraves da chamada activate
void task_passivate () ;

// acorda a tarefa indicada em deltaTime (pode ser zero, para acordar agora);
// Esta chamada somente acorda tarefas dormindo por task_passivate.
void task_activate (uint task_id, double deltaTime) ;

// torna passiva a tarefa indicada, que deve estar ativa (ready)
void task_cancel (uint task_id) ;

// retorna o ID da tarefa corrente
uint task_id () ;

//----------------------------------------------------------------------------

// Cria uma nova fila de mensagens, indicando o numero maximo de mensagens
// que ela suporta e a politica da fila (nesta versao, somente suporta
// a politica POLICY_FIFO e nao suporta capacidades finitas)
// retorno: o ID unico da fila
uint queue_create (uint capacity, uint policy) ;

// destroi uma fila de mensagens e todas as mensagens nela presentes
void queue_destroy (uint queue_id) ;

// retorna estatisticas de operacao da fila indicada
// queue_id : identificador unico da fila
// size: numero de mensagens na fila
// mean: media de mensagens na fila (tamanho medio da fila)
// var: variancia da media de mensagens na fila
// max: numero maximo de mensagens na fila
// put: numero de msgs colocadas na fila
// got: numero de msgs retiradas da fila
// parametros a ignorar podem ser informados como NULL
void queue_stats (uint queue_id, uint *size, uint *max,
                  double *mean, double *var,
                  ulong *put, ulong *got) ;

//----------------------------------------------------------------------------

// cria uma nova mensagem (o tipo eh definido pelo usuario, e pode
// conter as informacoes que bem desejar)
void* msg_create (ushort size) ;

// destroi uma mensagem (todas as msgs devem ser destruidas ao
// encerrar sua vida util, para liberar memoria)
void msg_destroy (void *msg) ;

// retorna um ponteiro para a primeira/ultima mensagem da fila
// ou NULL, caso a fila esteja vazia.
// IMPORTANTE: a mensagem nao eh removida da fila (use msg_pull)
void* msg_first (uint queue_id) ;
void* msg_last (uint queue_id) ;

// retorna um ponteiro para a mensagem anterior/proxima a uma dada msg,
// na mesma fila onde ela se encontra
// IMPORTANTE: a mensagem nao eh removida da fila
void* msg_prev (void* msg) ;
void* msg_next (void* msg) ;

// coloca (envia) uma mensagem msg na fila indicada por queue_id
// a mensagem msg nao pode estar em outra fila
void msg_put (uint queue_id, void* msg) ;

// puxa (retira) a mensagem da fila onde ela se encontra atualmente
void* msg_get (void* msg) ;

// recebe uma mensagem da fila queue_id, com time-out t
// (para ignorar o time-out, use a constante INFINITY)
// IMPORTANTE: a mensagem nao eh removida da fila (use msg_pull)
void* msg_wait (uint queue_id, double timeOut) ;

// informa atributos da mensagem indicada como parametro
// id : identificador unico da mensagem (ID)
// birth: data de criacao da mensagem
// sent: data de ultimo envio da mensagem
// creator: ID da tarefa que criou a mensagem
// sender: ID da tarefa que enviou a mensagem por ultimo
// queue: ID da fila onde msg se encontra, ou 0 se nao esta em nenhuma
// parametros a ignorar podem ser informados como NULL
void msg_attr (void *msg, ulong *id, double *birth, double *sent,
               uint *creator, uint *sender, uint *queue) ;

//----------------------------------------------------------------------------

#endif
