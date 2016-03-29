// Heap binario que implementa uma fila de prioridades. Os elementos do heap
// sao mantidos ordenados de acordo com suas datas crescentes. O primeiro
// elemento do heap indica o elemento com a menor data associada.
//
// Carlos Maziero, outubro de 2007

#ifndef HEAPLIB
#define HEAPLIB

//----------------------------------------------------------------------------

// flags de depuracao e checagem (usar -DFLAG na compilacao)
//
// HEAPCHECK  : verifica a integridade do heap a cada operacao
// HEAPDEBUG  : gera mensagens sobre operacoes no heap

//----------------------------------------------------------------------------

// estrutura de um elemento do heap binario
typedef struct helem_t {
  void*   value;     // valor armazenado no elemento
  double  time;      // data associada ao elemento
  int     rank;      // posicao do elemento no heap
} helem_t ;

// estrutura geral do heap binario
typedef struct heap_t {
  helem_t** elem;    // vetor de elementos
  int      size;     // numero de elementos
  int      maxSize;  // tamanho maximo do heap
} heap_t ;

//----------------------------------------------------------------------------

// inicia um heap binario
// retorno: 0 se ok, -1 se erro
int heap_init (heap_t* heap, int maxSize) ;

// destroi um heap binario, liberando a memoria usada
// retorno: 0 se ok, -1 se erro
int heap_kill (heap_t *heap) ;

// insere um novo elemento no heap
// retorno: ponteiro para o elemento, NULL se erro
helem_t* heap_insert (heap_t* heap, void* value, double time) ;

// remove um elemento do heap, liberando sua memoria
// retorno: 0 se ok, -1 se erro
void heap_delete (heap_t* heap, helem_t* elem) ;

// ajusta a posicao de um elemento de acordo com sua data
// retorno: 0 se ok, -1 se erro
void heap_adjust (heap_t* heap, helem_t* elem, double time) ;

// retorna um ponteiro ao primeiro elemento
// retorno: ponteiro ao elemento ou NULL, se fila vazia
helem_t* heap_first (heap_t* heap) ;

// retorna o numero de elementos no heap
// retorno: inteiro >= 0, ou -1 em caso de erro
int heap_size (heap_t* heap) ;

#endif

//----------------------------------------------------------------------------

