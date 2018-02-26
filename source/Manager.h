/*****************************************************************************
    Declaration of manager class that handles creation of workers and
    assignment of jobs

    Author:
    Jake McLeman

    All content copyright 2017 DigiPen (USA) Corporation, all rights reserved.
******************************************************************************/
#ifndef _MANAGER_H
#define _MANAGER_H

#include <condition_variable>
#include <mutex>
#include <vector>

#include "../includes/moodycamel/concurrentqueue.h"

#include "JobExceptions.h"
#include "Worker.h"

namespace JobBot
{
class Manager
{
public:
  /*
      Create a manager with the given number of workers

      If numWorkers is 0, system will start one worker for
      each available core in the machine
  */
  Manager(size_t numWorkers = 0);

  /*
      Shut down and join all the workers
  */
  ~Manager();

  /*
      Throw a job at the workers for them to complete
  */
  bool SubmitJob(Job* job);

  /*
      Get a worker based on its thread ID
  */
  Worker* GetWorkerByThreadID(std::thread::id id);

  /*
      Get the worker who's thread ID matches the current thread
  */
  Worker* GetThisThreadsWorker();

  /*
      Get a random worker (for stealing/assigning jobs)
  */
  Worker* GetRandomWorker();

  /*
      Get the worker that currently has the longest
      work queue.

      Guaranteed to always return a valid worker, but that
      worker may have an empty queue
  */
  Worker* GetBusiestWorker();

  /*
      Get the shared instance of the Manager
  */
  static Manager* GetInstance();

  /*
      Add a job to the singleton manager instance
  */
  static void RunJob(Job* job);

  /*
      Tell this threads worker to work while waiting for a job

      Will block until job is complete
  */
  static void WaitForJob(Job* job);

  /*
      Request a job for a worker with these parameters
  */
  Job* RequestJob(const Worker::Specialization& workerSpecialization);

  // Worker threads wait for this to tell them there are jobs
  std::condition_variable JobNotifier;

  /*
      Stop all worker threads associated with this manager
      Stop the workers and free their associated memory
  */
  void StopWorkers();

  /*
      Start up all workers, allocating memory and such as needed
      (The constructor calls this as part of the initialization,
      so it's only needed if you previously stopped the workers)
  */
  void StartWorkers();

private:
  /*
      Vector of the workers this manager is controlling

      May be better to have a vector of pointers to workers
      to prevent false sharing, speed testing is needed
  */
  std::vector<Worker*> workers_;

  /*
      Retain a list of all threads this manager has spun up
      for workers so that they can all be shut down cleanly
  */
  std::vector<std::thread> threads_;

  /*
      Mutex to make sure workers vector is not corrupted by
      multiple threads writing simultaneously
  */
  std::mutex workerMutex_;

  // If the workers are currently running
  std::atomic_bool workersWorking_;

  // Number of worker threads this manager should use
  const size_t numWorkers_;

  // Maximum length of a worker's queue
  static constexpr size_t sMaxWorkerQueueLength_ = 4096;

  // Array of specialized queues for each type of job
  moodycamel::ConcurrentQueue<Job*>
      jobs[static_cast<size_t>(JobType::NumJobTypes)];

  /*
      Attempt to get a job of specified type.

      Returns true if successful, false otherwise.
      If successful, pointer to retrived job is placed in 'job' reference.
      If unsuccessful, job is not modified
  */
  bool TryGetJob(JobType type, Job*& job);

  /*
      Function that will be spun up on threads for each worker
  */
  void StartNewWorker(Worker::Mode mode);
};

/*
Add a job to the singleton manager instance
*/
void RunJob(Job* job);

/*
Tell this threads worker to work while waiting for a job

Will block until job is complete
*/
void WaitForJob(Job* job);
}
#endif
