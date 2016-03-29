#include <stdio.h>
#include <stdlib.h>

#include "heap.h"

//----------------------------------------------------------------------------

// testa a integridade do heap periodicamente
#ifdef HEAPCHECK
#define CHECK(heap,msg)   heap_check (heap, msg, __FILE__, __LINE__)
#else
#define CHECK(heap,msg)
#endif

// mensagens de depuracao
#ifdef HEAPDEBUG
#define DEBUG(format,...)   fprintf (stderr, format, __VA_ARGS__) ;
#else
#define DEBUG(format,...)
#endif

// macro para testar condicoes de erro: se a condicao for verdadeira,
// imprime a mensagem de erro em stderr e aborta o programa com erro.
#ifdef NOTESTS
#define TEST(cond,...) 
#else
#define TEST(cond,...) \
if (cond) { \
   fprintf(stderr, "ERROR at %s:%d: ", __FILE__, __LINE__); \
   fprintf (stderr, __VA_ARGS__) ; \
   abort();\
}
#endif

//----------------------------------------------------------------------------

// vide fim do arquivo
void heap_check (heap_t *heap, char * msg, char * file, int line) ;
void heap_up (heap_t *heap, int position) ;
void heap_down (heap_t *heap, int position) ;

//----------------------------------------------------------------------------

// inicia um heap binario
// retorno: 0 se ok, -1 se erro

int heap_init (heap_t* heap, int maxSize)
{
   helem_t *elem ;

   // erro: heap nao existe
   TEST (!heap, "heap_init: heap nao existe\n") ;
   
   // guarda tamanho e tamanho inicial = 0
   heap->size = 0 ;
   heap->maxSize = maxSize ;

   // se heap ja existia, libera ele   
   if ( ! heap->elem )
     free (heap->elem) ;

   // aloca novo vetor de ponteiros de elementos, todos NULL
   heap->elem = (helem_t **) calloc (maxSize + 1, sizeof (helem_t *)) ;
   
   // erro: nao conseguiu alocar vetor de elementos
   TEST (!heap->elem, "heap_init: nao consegui alocar o heap\n") ;

   // cria um elemento nulo em heap->elem[0], com time = 0
   elem = malloc (sizeof(helem_t)) ;
   TEST (!elem, "heap_init: no memory for first element\n") ;
   
   // insere o elemento nulo no inicio do heap (simplifica os testes)
   heap->elem[0] = elem ;
   elem->rank = 0 ;
   elem->time = -1 ;
   elem->value = NULL ;

   CHECK(heap, "heap_init") ;
   
   return (0) ;
}

//----------------------------------------------------------------------------

// destroi um heap binario, liberando toda a memoria usada
// retorno: 0 se ok, -1 se erro

int heap_kill (heap_t *heap)
{
   int i ;
   
   // erro: heap nao existe
   TEST (!heap, "heap_kill: heap nao existe\n") ;

   // libera a memoria alocada
   if (heap->elem)
   {
     // libera elementos alocados
     for (i=0; i < heap->maxSize; i++)
       if (heap->elem[i])
         free (heap->elem[i]) ;

     // libera o vetor de ponteiros de elementos
     free (heap->elem) ;
     heap->elem = NULL ;
   }

   // heap nao contem elementos
   heap->size = 0 ;
   
   CHECK(heap, "heap_kill") ;
   
   return (0) ;
}

//----------------------------------------------------------------------------

// insere um novo elemento no heap
// retorno: ponteiro para o elemento, NULL se erro

helem_t* heap_insert (heap_t* heap, void* value, double time)
{
   helem_t* elem ;
   
   CHECK (heap, "heap_insert before") ;

   // o heap e o valor existem?
   TEST (!heap, "heap_insert: heap nao existe\n") ;
   TEST (!value, "heap_insert: value nao existe\n") ;

   // ha espaco para mais elementos no heap ?
   TEST (heap->size >= heap->maxSize, "heap_insert: no more place in heap\n") ;

   // aloca um novo elemento no heap
   elem = malloc (sizeof(helem_t)) ;
   TEST (!elem, "heap_insert: no memory for new elements\n") ;
   
   // insere o novo elemento no heap
   heap->size++ ;
   heap->elem[heap->size] = elem ;
   
   // preenche o novo elemento
   elem->rank = heap->size ;
   elem->time = time ;
   elem->value = value ;

   // reordena o heap
   heap_up (heap, heap->size) ;

   CHECK (heap, "heap_insert after") ;

   DEBUG ("heap_insert: insert value at time %10.3f\n", elem->time) ;

   return (elem) ;
}

//----------------------------------------------------------------------------

// retira um elemento do heap, liberando sua memoria
// retorno: 0 se ok, -1 se erro

void heap_delete (heap_t *heap, helem_t *elem)
{
   int position ;

   // verificacoes de consistencia
   TEST (!heap, "heap_delete: heap nao existe\n") ;   
   TEST (!elem, "heap_delete: elem nao existe\n") ;
   TEST (heap->size < 1, "heap_delete: heap estah vazio\n") ;
   TEST (elem->rank < 0, "heap_delete: elem nao pertence ao heap\n") ;
   
   CHECK (heap, "heap_delete before") ;   
   DEBUG ("heap_delete: delete value at time %10.3f\n", elem->time) ;
   
   // retira o elemento do heap e o elimina
   position = elem->rank ;
   free (elem) ;

   // reorganiza o heap
   heap->elem[position] = heap->elem[heap->size] ;
   heap->elem[position]->rank = position ;
   heap->elem[heap->size] = NULL ;
   heap->size-- ;
   heap_down (heap, position) ;

   CHECK (heap, "heap_delete after") ;
}

//----------------------------------------------------------------------------

// ajusta a posicao de um elemento de acordo com sua data
// retorno: 0 se ok, -1 se erro

void heap_adjust (heap_t* heap, helem_t* elem, double time)
{
   double oldTime ;

   // verificacoes de consistencia
   TEST (!heap, "heap_adjust: heap nao existe\n") ;   
   TEST (!elem, "heap_adjust: elem nao existe\n") ;
   
   CHECK (heap, "heap_adjust before") ;

   // atualiza a data da tarefa
   oldTime = elem->time ;
   elem->time = time ;

   DEBUG ("heap_adjust: time %0.6f -> %0.6f\n", oldTime, time) ;

   // reorganiza o heap
   if (time < oldTime)
     heap_up (heap, elem->rank) ;
   else
     heap_down (heap, elem->rank) ;
   
   CHECK (heap, "heap_adjust after") ;
}

//----------------------------------------------------------------------------

// retorna um ponteiro ao primeiro elemento
// retorno: ponteiro ao elemento ou NULL, se fila vazia

helem_t* heap_first (heap_t *heap)
{
   CHECK (heap, "heap_first") ;

   // verificacoes de consistencia
   TEST (!heap, "heap_adjust: heap nao existe\n") ;   

   return (heap->elem[1]) ;
}

//----------------------------------------------------------------------------

// retorna o numero de elementos no heap
// retorno: inteiro >= 0, ou -1 em caso de erro

int heap_size (heap_t *heap)
{
   // verificacoes de consistencia
   TEST (!heap, "heap_adjust: heap nao existe\n") ;   

   return (heap->size) ;
}

//----------------------------------------------------------------------------

// reorganiza o elemento "position" do heap em direcao a raiz
void heap_up (heap_t *heap, int position)
{
   helem_t *pivot ;

   TEST (position < 1 || position > heap->size,
         "heap_up: position %d is invalid\n", position) ;
   
   pivot = heap->elem[position] ;
   
   DEBUG ("heap_up: size: %d, pos: %d, pos/2: %d\n",
          heap->size, position, position/2) ;

   while (pivot->time < heap->elem[position/2]->time)
   {
      DEBUG ("heap_up: size: %d, pos: %d, pos/2: %d\n",
             heap->size, position, position/2) ;

      heap->elem[position] = heap->elem[position/2] ;
      heap->elem[position/2]->rank = position ;
      position = position/2 ;
   }
   heap->elem[position] = pivot ;
   pivot->rank = position ;
   
   CHECK (heap, "heap_up") ;
}

//----------------------------------------------------------------------------

// reorganiza o elemento "position" do heap em direcao as folhas

void heap_down (heap_t *heap, int position)
{
   int j ;
   helem_t *pivot ;
   
   TEST (position < 0 || position > heap->size,
         "heap_up]: position %d is invalid\n", position) ;

   pivot = heap->elem[position] ;
   
   while (position <= heap->size/2)
   {
      DEBUG ("heap_down: size: %d, pos: %d, pos/2: %d\n",
             heap->size, position, position/2) ;

      j = 2 * position ;
      if (j < heap->size)
	if (heap->elem[j]->time > heap->elem[j+1]->time)
	  j++ ;
      if (pivot->time <= heap->elem[j]->time)
	break ;
      heap->elem[position] = heap->elem[j] ;
      heap->elem[j]->rank = position ;
      position = j ;
   }  
   heap->elem[position] = pivot ;
   pivot->rank = position ;

   CHECK (heap, "heap_down") ;
}

//----------------------------------------------------------------------------

// verifica a estrutura do heap, buscando inconsistencias

void heap_check (heap_t *heap, char * msg, char* file, int line)
{
   int i, j, k ;
   
   // procura por entradas vazias no meio do heap
   for (i=1; i <= heap->size; i++)
     TEST (! heap->elem[i], "heap[%d] is empty (checked in %s:%d)\n",
           i, file, line) ;

   // verifica se as entradas do heap estao ordenadas   
   for (i=1; i<= heap->size; i++)
   {
      j = 2* i ;
      k = j + 1 ;
      
      TEST ((j <= heap->size) && (heap->elem[j]->time < heap->elem[i]->time),
            "heap is out of order (checked in %s:%d)\n", file, line) ;
      TEST ((k <= heap->size) && (heap->elem[k]->time < heap->elem[i]->time),
            "heap is out of order (checked in %s:%d)\n", file, line) ;
   }
}

//----------------------------------------------------------------------------

