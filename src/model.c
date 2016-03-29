#include <stdio.h>
#include <stdlib.h>
#include "simpatica.h"

// queue descriptor
int q1 ;

// variables to calculate mean of messages' lifetimes
long num_msgs = 0 ;
double sum_times = 0.0 ;

// messages are structs with user-defined fields
typedef struct msg_t
{
   int value ;  // a random value, just to give an example
} msg_t ;

// source tasks's body
void sourceBody (void *arg)
{
   msg_t *msg ;

   for (;;)
   {
      // creates a new message
      msg = (msg_t*) msg_create (sizeof (msg_t)) ;
       
      // fills the message with a random value
      msg->value = random ();
       
      // puts the message in q1 queue
      msg_put (q1, msg) ;
       
      // sleeps for a random amount of time
      task_sleep (15 + random() % 5) ;
   }
}

// sink task's body
void sinkBody (void *arg)
{
   msg_t *msg ;
   double creation_time ;

   for (;;)
   {
      // waits for a message and removes it from q1 queue
      msg = (msg_t*) msg_get (msg_wait (q1, INFINITY)) ;
       
      // gets the message creation date
      msg_attr (msg, 0, &creation_time, 0, 0, 0, 0) ;

      // simulation time elapsed for processing the message
      task_sleep (1) ;
               
      // accumulate times
      sum_times += (time_now() - creation_time) ;
      num_msgs ++ ;

      // destroys the message to free its resources
        msg_destroy (msg) ;
   }
}

int main ()
{
   int i ;
   double mean, variance ;

   // prepares a simulation for 1001 tasks and one queue 
   init_simulation (1001,1) ;

   // creates 1000 "source" tasks
   for (i=0; i< 1000; i++)
      task_create (sourceBody, NULL, 2) ;

   // creates one "sink" task
   task_create (sinkBody, NULL, 2) ;

   // creates the q1 queue
   q1 = queue_create (0, 0) ;

   // executes the simulation until t=50000 time units
   run_simulation (50000) ;

   // print results
   printf ("Mean time between msg production/consumption: %0.3f\n",
           sum_times / num_msgs) ;

   // prints mean queue size and its variance
   queue_stats (q1, 0, 0, &mean, &variance, 0, 0) ;
   printf ("Queue size: mean %0.3f, variance %0.3f\n", mean, variance) ;

   // frees simulation resources
   kill_simulation () ;

   exit(0) ;
} ;
