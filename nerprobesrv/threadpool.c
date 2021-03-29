/*
 * Copyright (c) 2016, Mathias Brossard <mathias@brossard.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file threadpool.c
 * @brief Threadpool implementation file
 */

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <stdio.h>
#include <iostream>
#include <chrono>

using namespace std;
using namespace std::chrono;

#include "threadpool.h"
// #include "server.h"

typedef enum
{
  immediate_shutdown = 1,
  graceful_shutdown = 2
} threadpool_shutdown_t;

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

typedef struct
{
  void (*function)(void *);
  void *argument;
} threadpool_task_t;

/**
 *  @struct threadpool
 *  @brief The threadpool struct
 *
 *  @var notify       Condition variable to notify worker threads.
 *  @var threads      Array containing worker threads ID.
 *  @var thread_count Number of threads
 *  @var queue        Array containing the task queue.
 *  @var queue_size   Size of the task queue.
 *  @var head         Index of the first element.
 *  @var tail         Index of the next element.
 *  @var count        Number of pending tasks
 *  @var shutdown     Flag indicating if the pool is shutting down
 *  @var started      Number of started threads
 *  @var pended      Number of pending threads
 */
struct threadpool_t
{
  pthread_mutex_t lock;
  pthread_cond_t notify;
  pthread_t *threads;
  threadpool_task_t *queue;
  high_resolution_clock::time_point start;
  int thread_count;
  int queue_size;
  int head;
  int tail;
  int count;
  int shutdown;
  int started;
  int pended;
  int stat;
  pthread_t *stat_thread;
  int threadTCP;
  int threadUDP;
  pthread_t *shrink_thread;
  int exit_thread;
  pthread_t *enlarge_thread;
};

/**
 * @function void *threadpool_thread(void *threadpool)
 * @brief the worker thread
 * @param threadpool the pool which own the thread
 */
static void *threadpool_thread(void *threadpool);

int threadpool_free(threadpool_t *pool);

/*Does the thread alive*/
int is_thread_alive(pthread_t tid)
{
  int kill_rc = pthread_kill(tid, 0); //Send signal 0 to test whether it is alive
  if (kill_rc == ESRCH)               //Thread does not exist
  {
    return false;
  }
  return true;
}

void *threadpool_status(void *threadpool)
{
  threadpool_t *pool = (threadpool_t *)threadpool;
  // timer for update interval
  auto updateStart = high_resolution_clock::now();
  while (1)
  {
    auto updateStop = high_resolution_clock::now();
    auto updateDuration = duration_cast<milliseconds>(updateStop - updateStart);
    // chceck if update display
    if (updateDuration.count() >= pool->stat)
    {
      auto stop = high_resolution_clock::now();
      auto duration = duration_cast<milliseconds>(stop - pool->start);

      cout << "\r"
           << "Elapsed [" << duration.count() / 1000 << "s] Threadpool [" << pool->thread_count << "|" << pool->started << "] TCP Clients[" << pool->threadTCP << "] UDP Clients [" << pool->threadUDP << "]             " << flush;
    }

    if ((pool->shutdown == immediate_shutdown) ||
        ((pool->shutdown == graceful_shutdown) &&
         (pool->count == 0)))
    {
      break;
    }
  }

  pthread_exit(NULL);
  return (NULL);
}

void *threadpool_shrink(void *threadpool)
{
  threadpool_t *pool = (threadpool_t *)threadpool;

  bool shrinkFlag;
  int cur_thread_count;
  auto shrinkStart = high_resolution_clock::now();
  auto shrinkStop = high_resolution_clock::now();
  auto shrinkDuration = duration_cast<milliseconds>(shrinkStop - shrinkStart);

  while (1)
  {
    if (pool->started < pool->thread_count / 2 && pool->thread_count > 1)
    {
      shrinkFlag = true;
      cur_thread_count = pool->thread_count;
      shrinkStart = high_resolution_clock::now();
      while (1)
      {
        shrinkStop = high_resolution_clock::now();
        shrinkDuration = duration_cast<milliseconds>(shrinkStop - shrinkStart);
        if (cur_thread_count != pool->thread_count)
        {
          shrinkFlag = false;
          break;
        }
        if (shrinkDuration.count() >= 60000)
          // if (shrinkDuration.count() >= 3000)
          // {
          break;
      }
    }
    if (!shrinkFlag)
    {
      continue;
    }
    else
    {
      while (pthread_mutex_lock(&(pool->lock)) != 0)
      {
        // empty
      }

      if (pool->started >= pool->thread_count / 2)
      {
        pthread_mutex_unlock(&(pool->lock));
        continue;
      }

      cur_thread_count = pool->thread_count;

      pool->exit_thread = cur_thread_count / 2;
      pthread_mutex_unlock(&(pool->lock));

      while (pool->exit_thread > 0)
      {
        pthread_cond_signal(&(pool->notify));
      }
    }

    if ((pool->shutdown == immediate_shutdown) ||
        ((pool->shutdown == graceful_shutdown) &&
         (pool->count == 0)))
    {
      break;
    }
  }

  pthread_exit(NULL);
  return (NULL);
}

void *threadpool_enlarge(void *threadpool)
{
  threadpool_t *pool = (threadpool_t *)threadpool;

  while (1)
  {
    int cur_thread_count = pool->thread_count;
    while (pthread_mutex_lock(&(pool->lock)) != 0)
    {
      // empty
    }

    if (pool->started >= pool->thread_count && pool->thread_count < 512 && pool->thread_count > 0)
    {
      int i = 0;
      while (pool->thread_count < cur_thread_count * 2)
      {
        if (pool->threads[i] == 0 || !is_thread_alive(pool->threads[i]))
        {
          if (pthread_create(&(pool->threads[i]), NULL,
                             threadpool_thread, (void *)pool) != 0)
          {
            cout << endl
                 << "Exit: pthread_create() on thread[" << i << "] failed" << endl;
            pool->shutdown = 1;
            pthread_mutex_unlock(&(pool->lock));
            break;
          }
          pool->thread_count++;
          pool->pended++;
        }
        i++;
      }
    }
    pthread_mutex_unlock(&pool->lock);
    if ((pool->shutdown == immediate_shutdown) ||
        ((pool->shutdown == graceful_shutdown) &&
         (pool->count == 0)))
    {
      break;
    }
  }
  pthread_exit(NULL);
  return (NULL);
}

threadpool_t *threadpool_create(int thread_count, int queue_size, int flags, int stat)
{
  threadpool_t *pool;
  int i;
  (void)flags;

  if (thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE)
  {
    return NULL;
  }

  if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL)
  {
    goto err;
  }

  /* Initialize */
  pool->thread_count = 0;
  pool->queue_size = queue_size;
  pool->head = pool->tail = pool->count = 0;
  pool->shutdown = pool->started = 0;
  pool->stat = stat;
  pool->threadTCP = pool->threadUDP = 0;
  pool->exit_thread = 0;
  pool->start = high_resolution_clock::now();

  /* Allocate thread and task queue */
  pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * 512);
  pool->queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);
  pool->stat_thread = (pthread_t *)malloc(sizeof(pthread_t));
  pool->shrink_thread = (pthread_t *)malloc(sizeof(pthread_t));
  pool->enlarge_thread = (pthread_t *)malloc(sizeof(pthread_t));

  /* Initialize mutex and conditional variable first */
  if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
      (pthread_cond_init(&(pool->notify), NULL) != 0) ||
      (pool->threads == NULL) ||
      (pool->queue == NULL) ||
      (pool->stat_thread == NULL) ||
      (pool->shrink_thread == NULL) ||
      (pool->enlarge_thread == NULL))
  {
    goto err;
  }

  if (pthread_create(pool->stat_thread, NULL,
                     threadpool_status, (void *)pool) != 0)
  {
    cout << "Exit: pthread_create() on threadpool_status failed" << endl;
    threadpool_destroy(pool, 0);
    return NULL;
  }

  if (pthread_create(pool->shrink_thread, NULL,
                     threadpool_shrink, (void *)pool) != 0)
  {
    cout << "Exit: pthread_create() on threadpool_shrink failed" << endl;
    threadpool_destroy(pool, 0);
    return NULL;
  }

  if (pthread_create(pool->enlarge_thread, NULL,
                     threadpool_enlarge, (void *)pool) != 0)
  {
    cout << "Exit: pthread_create() on threadpool_enlarge failed" << endl;
    threadpool_destroy(pool, 0);
    return NULL;
  }

  /* Start worker threads */
  for (i = 0; i < thread_count; i++)
  {
    if (pthread_create(&(pool->threads[i]), NULL,
                       threadpool_thread, (void *)pool) != 0)
    {
      cout << "Exit: pthread_join() on thread[" << i << "] failed" << endl;
      threadpool_destroy(pool, 0);
      return NULL;
    }
    pool->thread_count++;
    pool->pended++;
  }

  return pool;

err:
  if (pool)
  {
    threadpool_free(pool);
  }
  return NULL;
}

int threadpool_add(threadpool_t *pool, void (*function)(void *),
                   void *argument, int flags)
{
  int err = 0;
  int next;
  (void)flags;

  if (pool == NULL || function == NULL)
  {
    return threadpool_invalid;
  }

  if (pthread_mutex_lock(&(pool->lock)) != 0)
  {
    return threadpool_lock_failure;
  }

  next = (pool->tail + 1) % pool->queue_size;

  do
  {
    /* Are we full ? */
    if (pool->count == pool->queue_size)
    {
      err = threadpool_queue_full;
      break;
    }

    /* Are we shutting down ? */
    if (pool->shutdown)
    {
      err = threadpool_shutdown;
      break;
    }

    /* Add task to queue */
    pool->queue[pool->tail].function = function;
    pool->queue[pool->tail].argument = argument;
    pool->tail = next;
    pool->count += 1;

    /* pthread_cond_broadcast */
    if (pthread_cond_signal(&(pool->notify)) != 0)
    {
      err = threadpool_lock_failure;
      break;
    }
  } while (0);

  if (pthread_mutex_unlock(&pool->lock) != 0)
  {
    err = threadpool_lock_failure;
  }

  return err;
}

int threadpool_destroy(threadpool_t *pool, int flags)
{
  int i, err = 0;

  if (pool == NULL)
  {
    return threadpool_invalid;
  }

  if (pthread_mutex_lock(&(pool->lock)) != 0)
  {
    return threadpool_lock_failure;
  }

  cout << "System shutting down..." << endl;

  do
  {
    /* Already shutting down */
    if (pool->shutdown)
    {
      err = threadpool_shutdown;
      break;
    }

    pool->shutdown = (flags & threadpool_graceful) ? graceful_shutdown : immediate_shutdown;

    /* Wake up all worker threads */
    if ((pthread_cond_broadcast(&(pool->notify)) != 0) ||
        (pthread_mutex_unlock(&(pool->lock)) != 0))
    {
      err = threadpool_lock_failure;
      break;
    }
    /* Join all worker thread */
    if (pthread_join(*(pool->stat_thread), NULL) != 0)
    {
      err = threadpool_thread_failure;
    }

    if (pthread_join(*(pool->shrink_thread), NULL) != 0)
    {
      err = threadpool_thread_failure;
    }

    if (pthread_join(*(pool->enlarge_thread), NULL) != 0)
    {
      err = threadpool_thread_failure;
    }

    for (i = 0; i < pool->thread_count; i++)
    {
      if (pthread_join(pool->threads[i], NULL) != 0)
      {
        err = threadpool_thread_failure;
      }
    }
  } while (0);

  /* Only if everything went well do we deallocate the pool */
  if (!err)
  {
    threadpool_free(pool);
  }
  return err;
}

int threadpool_free(threadpool_t *pool)
{
  if (pool == NULL || pool->started > 0)
  {
    return -1;
  }

  /* Did we manage to allocate ? */
  if (pool->threads)
  {
    free(pool->threads);
    free(pool->queue);

    /* Because we allocate pool->threads after initializing the
           mutex and condition variable, we're sure they're
           initialized. Let's lock the mutex just in case. */
    pthread_mutex_lock(&(pool->lock));
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
  }
  free(pool);
  return 0;
}

static void *threadpool_thread(void *threadpool)
{
  threadpool_t *pool = (threadpool_t *)threadpool;
  threadpool_task_t task;
  struct arguments temp;

  for (;;)
  {
    /* Lock must be taken to wait on conditional variable */
    pthread_mutex_lock(&(pool->lock));

    /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
    while ((pool->count == 0) && (!pool->shutdown)) // && pool->shrink == 0)
    {
      pthread_cond_wait(&(pool->notify), &(pool->lock));

      /* Whether there are threads that need to be cleared */
      if (pool->exit_thread > 0)
      {
        pool->exit_thread--;
        pool->pended--;
        pool->thread_count--;
        pthread_mutex_unlock(&(pool->lock));
        pthread_exit(NULL);
      }
    }

    if (((pool->shutdown == immediate_shutdown) ||
         ((pool->shutdown == graceful_shutdown) &&
          (pool->count == 0))))
    {
      break;
    }

    /* Grab our task */
    task.function = pool->queue[pool->head].function;
    task.argument = pool->queue[pool->head].argument;
    pool->head = (pool->head + 1) % pool->queue_size;
    pool->count -= 1;

    pool->pended--;
    pool->started++;

    memcpy(&temp, task.argument, sizeof(temp));
    if (temp.proto == TCP)
    {
      pool->threadTCP++;
    }
    else
    {
      pool->threadUDP++;
    }
    /* Unlock */
    pthread_mutex_unlock(&(pool->lock));
    /* Get to work */
    (*(task.function))(task.argument);
    /* Lock must be taken to wait on conditional variable */
    pthread_mutex_lock(&(pool->lock));
    pool->started--;
    pool->pended++;

    if (temp.proto == TCP)
    {
      pool->threadTCP--;
    }
    else
    {
      pool->threadUDP--;
    }

    /* Unlock */
    pthread_mutex_unlock(&(pool->lock));
  }
  pthread_mutex_unlock(&(pool->lock));
  pthread_exit(NULL);
  return (NULL);
}
