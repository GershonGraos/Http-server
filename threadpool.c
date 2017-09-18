#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "threadpool.h"

//***************************************************************
                 //  Private Functions //
//***************************************************************

void enqueue(threadpool* from_me , work_t* work);
work_t* dequeue(threadpool* from_me);

//***************************************************************
                  // Create Threads //
//***************************************************************
threadpool* create_threadpool(int num_threads_in_pool)
{
    if(num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1 )
    {
        printf("\nNumber of threads is illegal\n");
        return NULL;
    }

    threadpool* t_pool;
    t_pool = (threadpool*) malloc (sizeof(threadpool));
    if(t_pool == NULL)
    {
        perror("\nError-Dynamic allocation failed\n");
        return NULL;
    }    

    t_pool->num_threads = num_threads_in_pool;
    t_pool->shutdown = 0;
    t_pool->dont_accept = 0;

    // QUEUE //
    t_pool->qsize = 0;  //size of the queue
    t_pool->qhead = NULL; //queue head pointer
    t_pool->qtail = NULL; //queue tail pointer

    t_pool->threads = (pthread_t*)malloc(num_threads_in_pool * sizeof(pthread_t) );
    if(t_pool->threads == NULL)
    {
        perror("\nError-Dynamic allocation failed\n");
        return NULL;
    } 

    if( pthread_mutex_init(&t_pool->qlock , NULL) != 0 )
    {
        fprintf(stderr, "\nMutex init failed\n");
        return NULL;
    }

    if( pthread_cond_init(&t_pool->q_not_empty , NULL) != 0 )
    {
        fprintf(stderr, "\nCondition init failed\n");
        return NULL;
    }

    if( pthread_cond_init(&t_pool->q_empty, NULL) != 0 )
    {
        fprintf(stderr, "\nCondition init failed\n");
        return NULL;
    }

    int i;
    int err;

    // CREATE THREADS //
    for(i=0; i<num_threads_in_pool; i++)
    {
        err = pthread_create(t_pool->threads+i, NULL, do_work, t_pool);

        if(err != 0)
            printf("\nCan't create thread:[%s]", strerror(err));
        else
            printf("\nThread created successfully");
    }

    return t_pool;
}


//******************************************************************
                  // Add work to the queue //
//******************************************************************
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void* arg)
{
    if(from_me == NULL)
        return;
    
    if(dispatch_to_here == NULL)
        return;    

	pthread_mutex_lock(&from_me->qlock); // add item to the queue so i need to LOCK
	if(from_me->dont_accept == 1)
	{
		printf("\nDestruction function\n");
		pthread_mutex_unlock(&from_me->qlock);
		return;
	}

	work_t* work;
	work = (work_t*)malloc (sizeof(work_t));
	if(work == NULL)
	{
		printf("\nNo assignal malloc to the object\n");
		return;
	}
	work->routine = dispatch_to_here;  // pointer to function
	work->arg = arg;
	work->next = NULL;

	enqueue(from_me , work);  // add item to the queue
	pthread_cond_signal(&from_me->q_empty); // te despierto xq ahora ya esta vacio
	pthread_mutex_unlock(&from_me->qlock); // te abro la puerta para q puedas trabajar(cuando hay muchos q desperte solo uno de ellos entrara)
}



//******************************************************************
           // Take work of the queue and Work //
//******************************************************************
void* do_work(void* p)
{
    if(p == NULL)
        return 0;
        
    threadpool* pool = (threadpool*) p; // pointer to the t_pool, yo recibo el mismo 'pool' en esta funcion
    if(pool == NULL)
        return 0;

    while(1)
    {
        pthread_mutex_lock(&pool->qlock); // LOCK

        if(pool->shutdown == 1)
	    {   
		    printf("\nThe pool is in destroy\n");
		    pthread_mutex_unlock(&pool->qlock);
		    return 0;
	    }

        
        if(pool->qsize == 0)
            pthread_cond_wait(&pool->q_empty, &pool->qlock);   
           
        if(pool->shutdown == 1)
	    {   
		    printf("\nThe pool is in destroy\n");
		    pthread_mutex_unlock(&pool->qlock);
		    return 0;
	    }            

	    work_t* work = dequeue(pool); // take work from the queue

        if(pool->qsize == 0) // if the queue is empty so wake up the flag not_empty ???
	        pthread_cond_signal(&pool->q_not_empty);

	    pthread_mutex_unlock(&pool->qlock); //UNLOCK
        work->routine(work->arg);
        free(work);
    }
}



//***************************************************************
            // Destroy the thread pool //
//***************************************************************
void destroy_threadpool(threadpool* destroyme)
{
    if(destroyme == NULL)
        return;
        
    int i;
    pthread_mutex_lock(&destroyme->qlock); // LOCK
    
    destroyme->dont_accept = 1;

    // wait until the thread will come //
    if(destroyme->qsize != 0)
        pthread_cond_wait(&destroyme->q_not_empty, &destroyme->qlock);

    destroyme->shutdown = 1;

    pthread_cond_broadcast(&destroyme->q_empty);  // wake up the threads
    pthread_mutex_unlock(&destroyme->qlock);  // UNLOCK

    for(i=0; i< (destroyme->num_threads); i++)
    {
        int err = pthread_join( destroyme->threads[i] , NULL );
        if(err != 0)
            perror("\nError in the join\n");
    }

    for(i=0; i<destroyme->qsize; i++)
    {
        free( (pthread_t*)destroyme->threads[i] );
    }

    free(destroyme->threads);
    free(destroyme);
}



//***************************************************************
            // Add a work at the end of my queue //
//***************************************************************
void enqueue(threadpool* from_me , work_t* work)
{
	work_t* temp;

	if(from_me->qsize > 0) // there is queue
	{
		temp = from_me->qtail;
		temp->next = work;
		from_me->qtail = work;
		from_me->qsize++;
	}

	else  // there is not queue
	{
		from_me->qhead = work;
		from_me->qtail = work;
		from_me->qsize = 1;
	}
}



//*******************************************************************
             // Pop the first work from the queue //
//*******************************************************************
work_t* dequeue(threadpool* from_me)
{
	work_t* stay;  // to pop the first item
	work_t* temp;

	// there is queue
	temp = from_me->qhead;
    stay = from_me->qhead;

	temp =  temp->next;  // promote the pointer
	from_me->qhead = temp;
	stay->next = NULL;

	if(from_me->qsize == 1)
	    from_me->qtail = NULL;

	from_me->qsize--;
	return stay;
}
