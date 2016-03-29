#include <stdio.h>
#include <stdlib.h>
#include "simpatica.h"

int queue ;

// variaveis para o calculo do tempo medio entre geracao e consumo das msgs
long num_msgs = 0 ;
double soma_tempos = 0.0 ;

// mensagens sao structs com conteudo definido pelo programador
typedef struct msg_t
{
   int value ;
} msg_t ;

// corpo das tarefas "source"
void sourceBody (void *arg)
{
   msg_t *msg ;
  
   for (;;)
   {
      // cria uma nova mensagem
      msg = (msg_t*) msg_create (sizeof (msg_t)) ;
     
      // preenche a mensagem com um valor aleatorio
      msg->value = random ();
     
      // coloca a mensagem na fila "queue"
      msg_put (queue, msg) ;
     
      // dorme durante um tempo aleatorio
      task_sleep (15 + random() % 5) ;
   }
}

// corpo da tarefa "sink"
void sinkBody (void *arg)
{
   msg_t *msg ;
   double data_criacao ;
  
   for (;;)
   {
      // espera e retira uma mensagem da fila
      msg = (msg_t*) msg_get (msg_wait (queue, INFINITY)) ;
     
      // obtem a data de criacao da mensagem
      msg_attr (msg, 0, &data_criacao, 0, 0, 0, 0) ;

      // simula o tempo gasto no tratamento da mensagem
      task_sleep (1) ;
                 
      // acumula tempos
      soma_tempos += (time_now() - data_criacao) ;
      num_msgs ++ ;

      // destroi a mensagem recebida (libera recursos)
      msg_destroy (msg) ;
   }
}

int main ()
{
   int i ;
   double media, variancia ;

   // prepara a simulacao para 1001 tarefas e uma fila  
   init_simulation (1001,1) ;
  
   // cria 1000 tarefas "source"
   for (i=0; i< 1000; i++)
     task_create (sourceBody, NULL, 2) ;

   // cria uma tarefa "sink"  
   task_create (sinkBody, NULL, 2) ;

   // cria uma fila "queue"
   queue = queue_create (0, 0) ;

   // executa a simulacao ate 50000 segundos
   run_simulation (50000) ;

   // imprime resultados obtidos
   printf ("Tempo medio entre producao e consumo das mensagens: %0.3f\n",
           soma_tempos / num_msgs) ;

   // imprime o tamanho medio da fila e seu desvio padrão
   queue_stats (queue, 0, 0, &media, &variancia, 0, 0) ;
   printf ("Tamanho da fila: media %0.3f, variancia %0.3f\n", media, variancia) ;

   // libera os recursos da simulacao
   kill_simulation () ;

   exit(0) ;
} ;
