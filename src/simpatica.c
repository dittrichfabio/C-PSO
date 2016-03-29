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

// Versao 0.7, 06/out/2007

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <time.h>
#include <malloc.h>
#include <math.h>

#include "heap.h"
#include "simpatica.h"

//----------------------------------------------------------------------------

// flags de depuracao e checagem (usar -DFLAG na compilacao)
//
// STACKCHECK : verifica a integridada da pilha a cada operacao
// STACKDEBUG : gera mensagens sobre uso das pilhas
// NOTESTS    : desliga todas as verificacoes de parametros das funcoes

//----------------------------------------------------------------------------

#ifdef __LP64__                        // plataforma 64 bits ?
#define CANARY    0xDEADBEEFDEADBEEF   // canario de pilha (dead beef) 64 bits
#else
#define CANARY    0xDEADBEEF           // canario de pilha (dead beef) 32 bits
#endif

// task states
#define RUNNING  0
#define READY    1
#define SLEEPING 2
#define WAITING  3
#define PASSIVE  4
#define DEAD     5

//----------------------------------------------------------------------------

// tipo "elemento de lista" generico.
// sera usado para fazer cast com task_t e message_t, portanto
// *prev e *next devem ser os primeiros campos nessas tres estruturas.
typedef struct void_t
{
   struct void_t *prev, *next ;
} void_t ;

//----------------------------------------------------------------------------

// tipo "mensagem"
typedef struct message_t
{
   struct message_t *prev, *next ;     // fila circular de mensagens
   ulong   id ;                        // identificador unico da msg
   double birth ;                      // data de criacao da mensagem
   double sent ;                       // data de ultimo envio da mensagem
   uint creator ;                      // tarefa que criou a mensagem
   uint sender ;                       // tarefa que enviou a mensagem por ultimo
   uint queue ;                        // ID da fila onde a mensagem estah
} message_t ;

//----------------------------------------------------------------------------

// tipo "tarefa"
typedef struct task_t 
{
   struct task_t *prev,*next ;     // permitem criar filas de tarefas
   uint id ;                       // identificador unico da task
   helem_t* hreg ;                 // registro da task no heap
   ucontext_t context ;            // informacao de contexto
   void *stack ;                   // aponta para a base da pilha
   ulong *canary ;                 // posicao do canario da pilha
   uchar2 status ;                  // status da task
   double awakeTime ;              // proxima data de acordar
} task_t ;

//----------------------------------------------------------------------------

// tipo "fila"
typedef struct queue_t 
{
  struct message_t *msg ;          // lista de mensagens da fila
  uint   id ;                      // identificador unico desta fila
  struct task_t *wait ;            // tarefas esperando mensagens da fila
  uint   size, max ;               // tamanhos atual e maximo da fila
  uint   put, got ;                // msgs colocadas/retiradas da fila
  double oldTime, sum, sum2 ;         // auxiliares para calcular media/desvio
} queue_t ;

//----------------------------------------------------------------------------

// variaveis do sistema operacional
ushort pageSize ;                       // tamanho de pagina da memoria

// controle de tarefas
task_t **taskVector ;                   // vetor de apontadores de tarefas
uint    maxTaskNumber ;                 // numero maximo de tarefas
uint    lastCreatedTask ;               // indice da ultima tarefa criada
uint    livingTasks ;                   // tarefas existentes (vivas)
task_t *currentTask ;                   // aponta para a task ativa no momento

// controle de filas
queue_t **queueVector ;                 // vetor de apontadores de filas
uint    maxQueueNumber ;                // numero maximo de filas
uint    lastCreatedQueue ;              // indice da ultima fila criada
queue_t *defaultQueue ;                 // fila default de mensagens

// controle do escalonador
task_t *dispatcher ;                    // tarefa que gerencia o escalonador
heap_t schedHeap ;                      // fila do escalonador

// controle de mensagens
ulong   messageCount ;                  // numero de mensagens criadas

// controle de tempo da simulacao
double simTime ;                        // relogio logico da simulacao
time_t startTime ;                      // instante de inicio da simulacao corrente
time_t currentTime;                     // instante atual da simulacao corrente

// controle de trace
ushort traceEnabled ;                   // trace esta ativado ?
ushort traceGenerate ;                  // gerar mensagens de trace ?
double traceStartTime, traceStopTime ;  // intervalo de tempo para trace

// controle de inicializacao da simulacao
ushort simInit = 0 ;                    // simulacao inicializada

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

// macro para testar condicoes de erro: se a condicao for verdadeira,
// imprime a mensagem de erro em stderr e aborta o programa com erro.
#ifdef NOTESTS
#define TEST(cond,...) 
#else
#define TEST(cond,...) \
if (cond) { \
  fprintf(stderr, "ERRO em %s:%d: ", __FILE__, __LINE__); \
  fprintf (stderr, __VA_ARGS__) ; \
  abort();\
}
#endif

//----------------------------------------------------------------------------

// testa a integridade da pilha da tarefa corrente
#ifdef STACKCHECK
#define CHECKSTACK   stack_check (currentTask, __FILE__, __LINE__)
#else
#define CHECKSTACK
#endif

// testa a integridade da pilha da tarefa corrente
void stack_check (task_t *task, char *file, int line) 
{
  TEST ((*task->canary) != CANARY, "pilha da tarefa %d corrompida\n", task->id) ;
}

//----------------------------------------------------------------------------

// macro para mensagens de trace
#define TRACE(...) \
if (traceGenerate) { \
  printf (">> %0.6g: ", simTime) ; \
  printf (__VA_ARGS__) ; \
}

// macro para mensagens de acompanhamento
#ifdef QUIET
#define MSG(...) 
#else
#define MSG(...) \
fprintf (stderr, "-- ") ; \
fprintf (stderr, __VA_ARGS__) ;
#endif

//----------------------------------------------------------------------------

// informa a memoria dinamica usada (heap), em Kbytes
uint memUse()
{
   struct mallinfo info = mallinfo() ;
   return ((info.usmblks + info.uordblks)/1024) ;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

// coloca um elemento generico no fim da fila
void queue_append (void_t **queue, void_t *elem)
{
   // testes de consistencia dos parametros
   TEST (! queue, "tentou inserir em uma lista inexistente\n") ;
   TEST (! elem, "tentou inserir um elemento inexistente\n") ;
   TEST (elem->prev || elem->next, "elemento ja esta em outra lista\n") ;
   
   if (! (*queue) ) // lista vazia
   {
      (*queue) = elem ;
      elem->prev = elem->next = elem ;
   }
   else // lista nao-vazia
   {
      elem->prev = (*queue)->prev ;
      elem->next = (*queue) ;
      elem->prev->next = elem ;
      elem->next->prev = elem ;
   }
}

//----------------------------------------------------------------------------

// retira um elemento generico da fila queue (nao apaga o elemento)
void queue_remove (void_t **queue, void_t *elem)
{
   // testes de consistencia dos parametros
   TEST (! queue, "tentou remover de uma lista inexistente\n") ;
   TEST (! (*queue), "tentou remover de uma lista vazia\n") ;
   TEST (! elem, "tentou remover um elemento inexistente\n") ;
   TEST (! elem->next, "tentou remover um elemento que nao esta em uma fila\n") ;

   // verifica se o elemento pertence a outra lista

   // atualiza os ponteiros dos vizinhos
    elem->prev->next = elem->next ;
    elem->next->prev = elem->prev ;

   // se remover o primeiro, o proximo sera o primeiro
   if (elem == (*queue))
      (*queue) = elem->next ;

   // se a lista soh tinha um elemento, fica vazia
   if (elem == (*queue))
      (*queue) = NULL ;
     
   // elemento removido nao deve ter vizinhos
   elem->next = elem->prev = NULL ;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

// cria uma nova tarefa, usando o parametro de entrada
uint task_create (void (*taskBody)(void *), void* startArg, ushort stackPages)
{
   task_t *task ;
   void *stack ;
   uint result ;
   uint stackSize ;

   // corpo da tarefa existe ?
   TEST (! taskBody, "task_create: task body is undefined\n") ;

   // ha espaco para novas tasks ?
   TEST (lastCreatedTask == maxTaskNumber - 1, "task_create: no more tasks\n") ;
   
   // tamanho minimo da pilha
   TEST (stackPages < 1, "task_create: stack size should be greater than zero\n") ;
   
   // aloca a pilha da tarefa
   stackSize = stackPages * pageSize ;
   stack = (void*) malloc (stackSize) ;
   TEST (!stack, "task_create: cannot allocate stack area for task\n") ;
   
   // aloca um descritor para a nova tarefa
   task = (task_t*) malloc (sizeof (task_t)) ;
   TEST (!task, "task_create: cannot allocate task descriptor\n") ;
   
   // cria um contexto para a nova tarefa
   result = getcontext (&task->context) ;
   TEST (result, "task_create: cannot create task context") ;
   
   // ajusta o contexto para usar a pilha recem-alocada
   task->context.uc_stack.ss_sp    = stack ;
   task->context.uc_stack.ss_size  = stackSize ;

   // se este contexto terminar, volta ao contexto do despachante
   task->context.uc_link = &dispatcher->context;

   // associa o novo contexto a funcao especificada como corpo
   makecontext (&task->context, (void*) (*taskBody), 1, startArg);

   // encontra um ID para a tarefa
   lastCreatedTask++ ;
   taskVector[lastCreatedTask] =  task ;
   livingTasks++ ;
   
   // preenche os dados da tarefa
   task->id = lastCreatedTask ;
   task->status = READY ;
   task->stack = stack ;
   task->awakeTime = simTime ;
   
   // define o "canario" da pilha da tarefa (um valor constante
   // colocado no final da pilha para verificar sua integridade.   
   // Afastei 1024 bytes do fim da pilha para aumentar sua sensibilidade
   //task->canary = (ulong*) (stack + stackSize - sizeof(ulong)) ;
   task->canary = (ulong*) (stack + 1024) ; 
   (*task->canary) = CANARY ;
   
   // coloca a tarefa na fila de escalonamento
   task->hreg = heap_insert (&schedHeap, task, task->awakeTime) ;
   
   // tracing de tarefas
   TRACE ("task %d created by task %d\n", task->id, currentTask->id) ;
   
   //retorna o ID da tarefa
   return (task->id) ;
}

//----------------------------------------------------------------------------

// encerra a tarefa corrente
void task_exit ()
{
   int result ;
   
   CHECKSTACK ;

   currentTask->status = DEAD ;
   
   // tracing de tarefas
   TRACE ("task %d exited\n", currentTask->id) ;

   // volta ao dispatcher
   result = swapcontext (&currentTask->context, &dispatcher->context) ;
   TEST (result, "task_exit: context switch") ;

   CHECKSTACK ;
}

//----------------------------------------------------------------------------

// destroi uma tarefa indicada por seu task ID, liberando a memoria
void task_destroy (uint tid)
{
   task_t * task ;
   
   task = taskVector[tid] ;

   // eh uma tarefa de usuario ?
   TEST (tid < 0, "task_destroy: not a valid task\n") ;
   
   // corpo da task existe ?
   if (! task)
     return ;

   // eh a tarefa corrente?
   TEST (task == currentTask, "task_destroy: cannot destroy myself\n") ;

   CHECKSTACK ;

   // retira a tarefa do escalonador, se estiver la
   if (task->hreg)
     heap_delete (&schedHeap, task->hreg) ;

   // libera a memoria da pilha da tarefa
   if (task->stack)
     free (task->stack) ;
     
   // libera a memoria do descritor da tarefa
   free (task) ;
   taskVector[tid] = NULL ;
   
   livingTasks-- ;

   // tracing de tarefas
   TRACE ("task %d destroyed by task %d\n", tid, currentTask->id) ;
   
   CHECKSTACK ;
}

//----------------------------------------------------------------------------

// informa o identificador unico da tarefa corrente
uint task_id ()
{
   return (currentTask->id) ;
}

//----------------------------------------------------------------------------

// retorna o relogio de simulacao atual
double time_now ()
{
   return simTime ;
}

//----------------------------------------------------------------------------

// tarefa corrente vai dormir t unidades de tempo
void task_sleep (double t)
{
   ushort result ;
   
   TEST ( t < 0, "task_sleep: task %d asked a negative sleeping time %0.6g\n",
	       currentTask->id, t) ;

   CHECKSTACK ;
   
   // registra data de acordar da tarefa
   currentTask->status = SLEEPING ;
   currentTask->awakeTime = simTime + t ;

   // tracing de tarefas
   TRACE ("task %d sleeps for %0.6g time units\n", currentTask->id, t) ;
	     
   CHECKSTACK ;
   
   // volta ao dispatcher
   result = swapcontext (&currentTask->context, &dispatcher->context) ;
   
   TEST (result, "task_sleep: context switch\n") ;

   CHECKSTACK ;
}

//----------------------------------------------------------------------------

// tarefa corrente vai dormir ate ser acordada por outra tarefa
void task_passivate ()
{
   ushort result ;
   
   CHECKSTACK ;
   
   // move a tarefa para o fim da fila do escalonador
   currentTask->status = PASSIVE ;
   currentTask->awakeTime = INFINITY ;

   // tracing de tarefas
   TRACE ("task %d passivated\n", currentTask->id) ;
	        
   CHECKSTACK ;

   // volta ao dispatcher
   result = swapcontext (&currentTask->context, &dispatcher->context) ;

   TEST (result, "task_passivate]: context switch\n") ;
   
   CHECKSTACK ;
}

//----------------------------------------------------------------------------

// a tarefa tid, que esta dormindo, deve acordar daqui a deltaTime
void task_activate (uint tid, double deltaTime)
{
   task_t *task = taskVector[tid] ;

   TEST (! task, "task_activate: task %d does not exist\n", tid) ;

   TEST (deltaTime < 0, "task_activate: task %d asked a negative activate time %0.6g\n",
         currentTask->id, deltaTime) ;
   
   CHECKSTACK ;
   
   // se a tarefa destino esta esperando, muda seu estado
   if (task->status == PASSIVE)
   {
      // acorda a tarefa
      task->status = READY ;
      task->awakeTime = simTime + deltaTime;
      
      // reposiciona a tarefa na fila do escalonador
      heap_adjust (&schedHeap, task->hreg, task->awakeTime) ;
      
      // tracing de tarefas
      TRACE ("task %d activated by task %d (in %0.6g time units\n",
             tid, currentTask->id, deltaTime) ;
   }   
   CHECKSTACK ;
}

//----------------------------------------------------------------------------

// torna passiva a tarefa indicada, que deve estar ativa (ready)
void task_cancel (uint tid)
{
   task_t *task = taskVector[tid] ;

   TEST (! task, "task_activate: task %d does not exist\n", tid) ;
   
   CHECKSTACK ;
   
   // se a tarefa destino esta ativa ou dormindo, muda seu estado
   if (task->status == READY || task->status == SLEEPING)
   {
      // ajusta o status da tarefa alvo
      task->status = PASSIVE ;
      task->awakeTime = INFINITY ;
   
      // move a tarefa para o fim da fila do escalonador
      heap_adjust (&schedHeap, task->hreg, task->awakeTime) ;

      // tracing de tarefas
      TRACE ("task %d cancelled by task %d\n", tid, currentTask->id) ;
   }   
   CHECKSTACK ;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

// Cria uma nova fila de mensagens, indicando o numero maximo de mensagens
// que ela suporta e a politica da fila (nesta versao, somente suporta
// a politica POLICY_FIFO)
// retorno: o ID unico da fila
uint queue_create (uint capacity, uint policy)
{
  queue_t * queue ;

  // ha espaco para novas tasks ?
  TEST (lastCreatedQueue == maxQueueNumber - 1, "queue_create: no more queues\n") ;
   
  // aloca um descritor para a nova tarefa
  queue = (queue_t*) malloc (sizeof (queue_t)) ;
  TEST (!queue, "queue_create: cannot allocate queue descriptor\n") ;
   
  // encontra um ID para a fila
  lastCreatedQueue++ ;
  queueVector[lastCreatedQueue] =  queue ;
   
  // preenche dados iniciais da fila
  queue->msg = NULL ;
  queue->id  = lastCreatedQueue;
  queue->wait = NULL ;
  queue->size = 0 ;
  queue->max  = 0 ;
  queue->put  = 0 ;
  queue->got  = 0 ;
  queue->oldTime = 0.0 ;
  queue->sum  = 0.0 ;
  queue->sum2 = 0.0 ;

  return (queue->id) ;
}

//----------------------------------------------------------------------------

// destroi uma fila de mensagens e todas as mensagens nela presentes
void queue_destroy (uint queue_id)
{
   queue_t   *queue ;
   message_t *msg_hdr ;
      
   // eh uma fila de usuario ?
   TEST (queue_id < 0, "queue_destroy: not a valid queue\n") ;
   
   queue = queueVector[queue_id] ;

   // corpo da task existe ?
   if (! queue)
     return ;

   CHECKSTACK ;

   // remove e elimina as mensagens da fila
   while ((msg_hdr = queue->msg))
   {
      queue_remove ((void_t**) &queue->msg, (void_t*) msg_hdr) ;
      free (msg_hdr) ;
   }
   
   // libera a memoria do descritor da fila
   free (queue) ;
   queueVector[queue_id] = NULL ;
   
   // tracing de filas
   TRACE ("queue %d destroyed by task %d\n", queue_id, currentTask->id) ;
   
   CHECKSTACK ;
}

//----------------------------------------------------------------------------

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
                  ulong *put, ulong *got)
{
   queue_t *queue ;
   
   // a fila existe ?
   TEST (queue_id < 1, "queue_stats: invalid queue\n") ;
   queue = queueVector[queue_id] ;
   TEST (! queue, "queue_stats: que does not exist\n") ;
         
   CHECKSTACK ;
   
   // informa/calcula as estatisticas solicitadas
   if (size)
      (*size) = queue->size ;
   if (max)
      (*max)  = queue->max ;
   if (mean)
      (*mean)  = queue->sum / simTime ;
   if (var)
      (*var)  = (queue->sum2 - queue->sum*queue->sum/simTime) / simTime ;
   if (put)
      (*put)  = queue->put ;
   if (got)
      (*got)  = queue->got ;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

// cria uma nova mensagem, especificando seu tamanho
void *msg_create (ushort size)
{
   message_t *msg_hdr ;
   
   // aloca espaco para a mensagem + header
   msg_hdr = (message_t*) malloc (sizeof(message_t) + size) ;
   TEST (! msg_hdr, "msg_create: erro no malloc\n") ;      

   CHECKSTACK ;
   
   // preenche o header da msg
   msg_hdr->next    = NULL ;
   msg_hdr->prev    = NULL ;
   msg_hdr->id      = messageCount++ ;
   msg_hdr->birth   = simTime ;
   msg_hdr->creator = currentTask->id ;
   
   // coloca a mensagem no fim da fila default 
   msg_hdr->queue   = 0 ;
   queue_append ((void_t**)&defaultQueue->msg, (void_t*) msg_hdr) ;

   // tracing de mensagens
   TRACE ("task %d creates msg %ld\n", currentTask->id, msg_hdr->id) ;
	     
   // esconde o header (avanca os bytes extras)
   msg_hdr++ ;

   return (msg_hdr) ;
}

//----------------------------------------------------------------------------

// destroi a mensagem indicada, liberando sua memoria
void msg_destroy (void *msg)
{
   message_t *msg_hdr ;
   
   // nao destroi mensagens nulas
   TEST (!msg, "msg_destroy: mensagem nula\n") ;
         
   CHECKSTACK ;
   
   // encontra o cabecalho da msg
   msg_hdr = (message_t*) msg ;
   msg_hdr-- ;
   
   TEST (msg_hdr->queue, "msg_destroy: msg is in a queue\n") ;

   // tracing de mensagens
   TRACE ("task %d destroys msg %ld\n", currentTask->id, msg_hdr->id) ;
	        
   // retira a msg da fila default e a elimina
   queue_remove ((void_t**) &defaultQueue->msg, (void_t*) msg_hdr) ;
   free (msg_hdr) ;
   
   CHECKSTACK ;
}

//----------------------------------------------------------------------------

// retorna um ponteiro para a primeira mensagem da fila
// ou NULL, caso a fila esteja vazia.
// IMPORTANTE: a mensagem nao eh removida da fila (use msg_pull)
void* msg_first (uint queue_id)
{
   message_t *msg_hdr ;
   queue_t *queue ;
   
   // a fila existe?
   queue = queueVector[queue_id] ;
   TEST (queue_id <= 0, "msg_first: invalid queue %d\n", queue_id) ;
   TEST (! queue, "msg_first: queue %d does not exist\n", queue_id) ;
   
   // encontra primeira msg da fila   
   msg_hdr = queue->msg ;

   // nao ha mensagem    
   if (!msg_hdr)
     return (NULL) ;
     
   // esconde o cabecalho da msg
   msg_hdr++ ;

   return (msg_hdr) ;
}

//----------------------------------------------------------------------------

// retorna um ponteiro para a ultima mensagem da fila
// ou NULL, caso a fila esteja vazia
// IMPORTANTE: a mensagem nao eh removida da fila (use msg_pull)
void* msg_last (uint queue_id)
{
   message_t *msg_hdr ;
   queue_t *queue ;
   
   // a fila existe?
   queue = queueVector[queue_id] ;
   TEST (queue_id <= 0, "msg_first: invalid queue %d\n", queue_id) ;
   TEST (! queue, "msg_first: queue %d does not exist\n", queue_id) ;
   
   // encontra primeira msg da fila   
   msg_hdr = queue->msg ;
   
   // nao ha mensagem    
   if (!msg_hdr)
     return (NULL) ;

   // encontra ultima mensagem (anterior da primeira)
   msg_hdr = msg_hdr->prev ;
   
   // esconde o cabecalho da msg
   msg_hdr++ ;

   return (msg_hdr) ;
}

//----------------------------------------------------------------------------

// retorna um ponteiro para a mensagem anterior a msg na fila onde m se encontra
// IMPORTANTE: a mensagem nao eh removida da fila (use msg_pull)
void *msg_prev (void *msg)
{
   message_t *msg_hdr ;
   
   // a msg existe ?
   TEST (!msg, "msg_attr: message pointer is null\n") ;
   
   // encontra o cabecalho da msg
   msg_hdr = (message_t*) msg ;
   msg_hdr-- ;
   
   CHECKSTACK ;

   // se a msg estiver na fila default (0), nao tem vizinhos
   if (!msg_hdr->queue)
     return (NULL) ;
     
   // se msg for a primeira, retorna NULL
   if (msg_hdr == queueVector[msg_hdr->queue]->msg)
     return (NULL) ;

   // encontra a anterior, esconde o cabecalho e retorna msg     
   msg_hdr = msg_hdr->prev ;
   msg_hdr++ ;
   return (msg_hdr) ;
}

//----------------------------------------------------------------------------

// retorna um ponteiro para a mensagem posterior a msg na fila onde m se encontra
// IMPORTANTE: a mensagem nao eh removida da fila (use msg_pull)
void *msg_next (void *msg)
{
   message_t *msg_hdr ;
   
   // a msg existe ?
   TEST (!msg, "msg_attr: message pointer is null\n") ;
   
   // encontra o cabecalho da msg
   msg_hdr = (message_t*) msg ;
   msg_hdr-- ;
   
   CHECKSTACK ;

   // se a msg estiver na fila default (0), nao tem vizinhos
   if (!msg_hdr->queue)
     return (NULL) ;
     
   // se msg for a ultima, retorna NULL
   if (msg_hdr == queueVector[msg_hdr->queue]->msg->prev)
     return (NULL) ;

   // encontra a proxima, esconde o cabecalho e retorna msg     
   msg_hdr = msg_hdr->next ;
   msg_hdr++ ;
   return (msg_hdr) ;
}

//----------------------------------------------------------------------------

// retira a mensagem da fila onde ela se encontra atualmente
void *msg_get (void *msg)
{
   message_t *msg_hdr ;
   queue_t   *queue ;
   double    interv ;
      
   // a msg existe ?
   TEST (!msg, "msg_attr: message pointer is null\n") ;
   
   // encontra o cabecalho da msg
   msg_hdr = (message_t*) msg ;
   msg_hdr-- ;
   
   CHECKSTACK ;

   // transfere a msg da fila atual para a fila default
   queue = queueVector[msg_hdr->queue] ;
   queue_remove ((void_t**) &queue->msg, (void_t*) msg_hdr) ;
   queue_append ((void_t**) &defaultQueue->msg, (void_t*) msg_hdr) ;
   msg_hdr->queue = 0 ;

   // ajusta contadores das filas
   interv = simTime - queue->oldTime ;
   queue->oldTime = simTime ;
   queue->sum  += interv * queue->size ;
   queue->sum2 += interv * queue->size * queue->size ;
   queue->size-- ;
   queue->got++ ;
   
   // esconde o cabecalho e retorna msg     
   msg_hdr++ ;
   return (msg_hdr) ;
}

//----------------------------------------------------------------------------

// coloca/envia uma mensagem para a fila indicada por queue_id
void msg_put (uint queue_id, void* msg)
{
   message_t *msg_hdr ;
   queue_t   *queue ;
   task_t    *task ;
   double    interv ;
      
   // a fila existe?
   TEST (queue_id <= 0, "msg_put: invalid queue %d\n", queue_id) ;
   queue = queueVector[queue_id] ;
   TEST (! queue, "msg_put: destination queue %d does not exist\n", queue_id) ;
   
   // a msg existe ?
   TEST (!msg, "msg_put: message to send does not exist\n") ;
   
   CHECKSTACK ;
   
   // encontra o cabecalho da msg
   msg_hdr = (message_t*) msg ;
   msg_hdr-- ;
   
   // a mensagem estah na fila default (livre) ?
   TEST (msg_hdr->queue, "msg_put: message is in a queue\n") ;
   
   // atualiza campos internos da msg
   msg_hdr->sent = simTime ;
   msg_hdr->sender = currentTask->id ;
   msg_hdr->queue = queue_id ;

   // transfere a msg da fila default para a fila destino
   queue_remove ((void_t**) &defaultQueue->msg, (void_t*) msg_hdr) ;
   queue_append ((void_t**) &queue->msg, (void_t*) msg_hdr) ;

   // ajusta contadores das filas
   interv = simTime - queue->oldTime ;
   queue->oldTime = simTime ;
   queue->sum  += interv * queue->size ;
   queue->sum2 += interv * queue->size * queue->size ;
   queue->size++ ;
   queue->put++ ;
   if (queue->size > queue->max)
     queue->max = queue->size ;
   
   // se alguma tarefa esta esperando naquela fila, a acorda
   if ((task = queue->wait))
   {
      // retira a tarefa da fila de espera
      queue_remove ((void_t**) &queue->wait, (void_t*) task) ;
      
      // acorda a tarefa
      task->status = READY ;
      task->awakeTime = simTime ;
      
      // reposiciona a tarefa na fila do escalonador
      heap_adjust (&schedHeap, task->hreg, task->awakeTime) ;
   }
  
   // tracing de mensagens
   TRACE ("task %d puts msg %ld on queue %d\n", currentTask->id,
          msg_hdr->id, queue_id) ;
	      
   CHECKSTACK ;
}

//----------------------------------------------------------------------------

// recebe uma mensagem da fila queue_id, com time-out t
// (para ignorar o time-out, use a constante INFINITY)
// IMPORTANTE: a mensagem nao eh removida da fila (use msg_pull)
void* msg_wait (uint queue_id, double timeOut)
{
   message_t *msg_hdr ;
   queue_t   *queue ;
   ushort result ;
   
   // time-outs devem ser positivos ou nulos 
   TEST (timeOut < 0, "msg_wait: task %d asked negative time-out %0.6g\n",
         currentTask->id, timeOut) ;

   CHECKSTACK ;
   
   // a fila existe?
   TEST (queue_id <= 0, "msg_wait: invalid queue %d\n", queue_id) ;
   queue = queueVector[queue_id] ;
   TEST (! queue, "msg_wait: destination queue %d does not exist\n", queue_id) ;
   
   TRACE ("task %d waits for msg on queue %d until t=%0.6g\n",
          currentTask->id, queue_id, timeOut) ;
   
   // se a fila estiver vazia e timeOut > 0, vai dormir
   if ((! queue->msg) && (timeOut > 0))
   {
      // registra data de acordar da tarefa
      currentTask->status = WAITING ;
      currentTask->awakeTime = simTime + timeOut ;
      
      // coloca tarefa na fila de espera associada a queue_id
      queue_append ((void_t**) &queue->wait, (void_t*) currentTask) ;
            
      // volta ao dispatcher
      result = swapcontext (&currentTask->context, &dispatcher->context) ;
      TEST (result, "msg_recv: context switch\n") ;
   }
   
   CHECKSTACK ;
   
   // a fila continua vazia (voltou por time-out) ?
   if (! queue->msg)
   {
     // tracing de mensagens
     TRACE ("task %d msg_wait time-out\n", currentTask->id) ;
		
     return (NULL) ;
   }
      
   // aponta a primeira msg da fila
   msg_hdr = queue->msg ;

   // tracing de mensagens
   TRACE ("task %d receives msg %ld\n", currentTask->id, msg_hdr->id) ;
	        
   // esconde o header (avanca o ponteiro)
   msg_hdr++ ;
   
   CHECKSTACK ;
   
   // retorna ponteiro para o elemento removido
   return (msg_hdr) ;
}

//----------------------------------------------------------------------------

// informa atributos da mensagem indicada como parametro
// id : identificador unico da mensagem (ID)
// birth: data de criacao da mensagem
// sent: data de ultimo envio da mensagem
// creator: ID da tarefa que criou a mensagem
// sender: ID da tarefa que enviou a mensagem por ultimo
void msg_attr (void *msg, ulong *id, double *birth, double *sent,
                 uint *creator, uint *sender, uint *queue)
{
   message_t *msg_hdr ;
   
   // a msg existe ?
   TEST (!msg, "msg_attr: message pointer is null\n") ;
   
   // encontra o cabecalho da msg
   msg_hdr = (message_t*) msg ;
   msg_hdr-- ;
   
   CHECKSTACK ;
   
   // copia os atributos da mensagem solicitados
   if (id)
      (*id) = msg_hdr->id ;
   if (birth)
      (*birth) = msg_hdr->birth ;
   if (sent)
      (*sent)  = msg_hdr->sent ;
   if (creator)
      (*creator) = msg_hdr->creator ;
   if (sender)
      (*sender)  = msg_hdr->sender ;
   if (queue)
      (*queue)  = msg_hdr->queue ;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

// inicia as estruturas de dados para uma nova simulacao
void init_simulation (uint maxTasks, uint maxQueues)
{
   ushort result ;

   // inicializa valores de controle
   simTime         = 0.0 ;
   livingTasks     = 0 ;
   lastCreatedTask = 0 ;
   lastCreatedQueue = -1 ;
   maxTaskNumber  = maxTasks + 1 ;  // soma o scheduler
   maxQueueNumber = maxQueues + 1 ; // soma a fila default

   // desligar o buffer do printf, que nao gosta de trocas de contexto
   setvbuf (stdout, 0, _IONBF, 0) ;
   
   // encontra o tamanho de pagina da maquina
   pageSize = sysconf (_SC_PAGESIZE) ;
   
   // aloca o vetor de ponteiros das tarefas 
   // (somei +1 task para incluir o scheduler, task [0])
   taskVector = (task_t**) calloc (maxTaskNumber+1, sizeof (task_t*)) ;
   TEST (! taskVector, "init_simulation: task vector allocation") ;

   // aloca um descritor separado para o dispatcher
   dispatcher = malloc (sizeof (task_t)) ;
   TEST (!dispatcher, "init_simulation: nao consegui alocar o dispatcher\n") ;
   
   // inicializa descritor do scheduler
   dispatcher->id = 0 ;
   dispatcher->status = RUNNING ;
   dispatcher->stack = NULL ;  // usa a pilha principal
   dispatcher->awakeTime = -1.0 ; // scheduler sempre fica no topo do heap
   currentTask = dispatcher ;
   
   // inicializa o contexto do scheduler   
   result = getcontext (&dispatcher->context) ;
   TEST (result, "init_simulation]: scheduler getcontext") ;
   
   // inicializa fila do scheduler
   heap_init (&schedHeap, maxTaskNumber) ;

   // aloca o vetor de ponteiros das filas
   // (somei +1 fila para incluir a fila default [0])
   queueVector = (queue_t**) calloc (maxQueueNumber+1, sizeof (queue_t*)) ;
   TEST (! queueVector, "init_simulation: queue vector allocation") ;

   // inicia a fila default de mensagens
   queue_create (0, 0) ;
   defaultQueue = queueVector[0] ;

   // simulacao inicializada
   simInit = 1 ;
   MSG ("Simulation initialized, Simpatica version %s (mem: %dKb)\n",
        SIMPATICA_VERSION, memUse()) ;
}

//----------------------------------------------------------------------------

// executa uma simulacao ate o tempo logico alcancar o valor indicado
void run_simulation (double thisMaxTime)
{
   uint result, percent, percentStep ;
   double maxTime, timeMark;
   ulong eventsCount = 0 ;
   task_t* nextTask ;
   
   percentStep = 10 ;
   percent = 0 ;
   startTime = time (0) ;

   TEST (! simInit, "Simulacao ainda nao foi inicializada\n") ;

   // define limite de tempo para esta simulacao
   maxTime  = simTime + thisMaxTime ;
   timeMark = simTime + percentStep * thisMaxTime / 100 ;
   percent  = 0 ;
   
   MSG ("Simulation in interval t=[%0.3f, %0.3f), %d tasks\n",
           simTime, thisMaxTime, livingTasks) ;

   do
   {
      // obtem primeira tarefa do escalonador
      nextTask = (heap_first (&schedHeap))->value ;
      
      // se houver eventos escalonaveis
      if ( nextTask )
      {
	 eventsCount++ ;
	 
         // avanca relogio de simulacao
	 TEST (simTime > nextTask->awakeTime,
	    "run_simulation]: time order violation at t=%0.6g\n", simTime) ;
         simTime = nextTask->awakeTime ;
	 
	 // gerar mensagens de trace ?
         if (traceEnabled)
         {  
            if (simTime > traceStopTime)
            {
               traceGenerate = 0 ;
               traceEnabled = 0 ;
            }
            else
               if (simTime >= traceStartTime)
                  traceGenerate = 1 ;         
         }
	 
         if (simTime <= maxTime)
         {
            // reativa a proxima tarefa
            nextTask->status = RUNNING ;
	    
	    currentTask = nextTask ;
            result = swapcontext (&dispatcher->context, &nextTask->context) ;
            TEST (result, "run_simulation]: context switch\n") ;

            // tarefa corrente volta a ser o dispatcher
            currentTask = dispatcher ;

            // verifica integridade da pilha da tarefa
	    stack_check (nextTask, __FILE__, __LINE__) ;

#ifdef STACKDEBUG
	    // imprime a pilha da tarefa
	    char *p ;
	    for (p = nextTask->stack ; p < (char*) nextTask->stack + 4096; p++)
	    { 
	       int val = (*p) & 0x000000FF ;
	       if (val)
		 printf ("%02X", val) ;
	       else
		 printf ("..") ;
	    }
	    printf ("\n\n") ;
#endif

            if (nextTask->status == DEAD)
  	      // tarefa finalizada, deve ser removida
              task_destroy (nextTask->id) ;
	    else
              // tarefa nao finalizada, deve ser reordenada no escalonador
              heap_adjust (&schedHeap, nextTask->hreg, nextTask->awakeTime) ;
	 }
         else
             // relogio de simulacao avanca ao fim da simulacao
             simTime = maxTime ;
      }
      else
         // relogio de simulacao avanca ao fim da simulacao
         simTime = maxTime ;
      
      // a cada 10% de avanco do tempo, informa ao usuario
      if ( simTime >= timeMark )
      {
	 timeMark += percentStep * thisMaxTime / 100 ;
	 percent  += percentStep ;
	 
	 // obtem o instante atual (tempo real)
	 currentTime = time (0) ;

	 // mensagem de progresso da simulacao
	 MSG ("Simulation time: %9.3f, %10li events, %3d%% done in %5.0f secs (mem: %dKb)\n",
		  simTime, eventsCount, percent,
		  difftime (currentTime, startTime), memUse()) ;
      }
   }
   while (simTime < maxTime) ;
   simTime = maxTime ;
   traceGenerate = 0 ;
   
   MSG ("Simulation completed in %1.0f seconds (mem: %dKb)\n",
        difftime (currentTime, startTime), memUse()) ;
}

//----------------------------------------------------------------------------

// remove da memoria todas as estruturas criadas para a ultima simulacao
void kill_simulation ()
{
   int i ;
   
   // reinicia a fila do escalonador
   heap_kill (&schedHeap) ;

   // libera todas as tarefas
   for (i = 0; i <= maxTaskNumber; i++)
     if (taskVector[i])
     {
       taskVector[i]->hreg = NULL ;
       task_destroy (i) ;
     }
   
   // libera o vetor de tarefas
   if (taskVector)
   {
      free (taskVector) ;
      taskVector = NULL ;
   }
   
   // libera todas as filas de mensagens
   for (i = 0; i <= maxQueueNumber; i++)
     queue_destroy (i) ;
   
   // libera o vetor de filas
   if (queueVector)
   {
      free (queueVector) ;
      queueVector = NULL ;
   }
   
   // libera a task do dispatcher
   if (dispatcher)
   {
      free (dispatcher) ;
      dispatcher = NULL ;
   }
   
   // limpa algumas variaveis importantes
   simTime          = 0.0 ;
   livingTasks      = 0 ;
   lastCreatedTask  = 0 ;
   maxTaskNumber    = 0 ;
   lastCreatedQueue = -1 ;
   maxQueueNumber   = 0 ;
   simInit          = 0 ;

   // simulacao terminada
   MSG ("Simulation killed (mem: %dKb)\n", memUse()) ;
}

//----------------------------------------------------------------------------

// define o intervalo no qual o tracing sera ativado
void trace_interval (double startTime, double stopTime)
{
   TEST ((startTime < 0) || (stopTime < startTime), "trace_interval: invalid values\n") ;
   traceStartTime = startTime ;
   traceStopTime = stopTime ;
   traceEnabled = 1 ;
   MSG ("Simulation trace enabled in t=[%0.3f, %0.3f]\n",
        startTime, stopTime) ;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

