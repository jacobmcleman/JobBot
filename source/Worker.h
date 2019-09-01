/**************************************************************************
Contains declaration for Worker class and its JobQueue subclass, which
handle job queues and stealing.

Author:
Jake McLeman

All content copyright 2017 DigiPen (USA) Corporation, all rights reserved.
**************************************************************************/
#ifndef _WORKER_H
#define _WORKER_H

#include "../includes/moodycamel/concurrentqueue.h"
#include <atomic>
#include <thread>

namespace JobBot
{
// Forward Declarations
class Job;
class Manager;

class Worker
{
public:
  // Description of the modes a worker can exist in
  enum struct Mode
  {
    Primary,
    Volunteer
  };

  /*
      Means of specifying types of work a worker should seek
  */
  struct Specialization
  {
    // Order in which workers with this specialization should request work
    JobType priorities[static_cast<size_t>(JobType::NumJobTypes) - 1];

    // Predefined specializations

    // Will take any work, prioritizing large but non-blocking work
    static Specialization None;
    // Takes blocking/IO jobs so other workers don't have to. Will take other
    // jobs after all IO is finished
    static Specialization IO;
    // Prefer completing graphics jobs, will only take small jobs so as to
    // always be ready for more graphics
    static Specialization Graphics;
    // Will take only tiny jobs, never accepts blocking jobs
    static Specialization RealTime;
  };

  /*
      Constructor for worker

      manager - this worker's manager that it may ask for jobs from
      maxJobs - the maxiumum jobs that can be accepted into this worker's queue
      mode - Mode this worker should operate as
  */
  Worker(Manager* manager, Mode mode, const Specialization& specialization);

  /*
      Copying or assigning to a worker does not make sense
  */
  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  /*
      Steal jobs from other workers and complete them while waiting for
      a job to complete

      job - The job to wait for
  */
  void WorkWhileWaitingFor(JobHandle job);

  /*
      Steal jobs from other workers and complete them while waiting for
      some condition to be true

      condition - the atomic condition variable that should be waited for, make
     sure that
      performing the check is thread safe
  */
  void WorkWhileWaitingFor(std::atomic_bool& condition);

  /*
      Start working on jobs from this worker's queue and stealing jobs if empty

      Basically just start doing work
  */
  void Start();

  /*
      Tell the worker not to start working on new jobs
      Blocks until the worker actually stops
  */
  void Stop();

  /*
      Politely ask the worker to stop. It will stop working
      and the thread will be joinable once its current
      task is complete
  */
  void StopAfterCurrentTask();

  /*
      Get the type of worker this is
  */
  Mode GetMode();

  /*
      Get the ID of the thread that this worker will work on
  */
  std::thread::id GetThreadID() const;

  /*
      Ask if the worker is currently working
  */
  bool IsWorking() const;

private:
  // This worker's manager
  Manager* manager_;
  // Mode that this worker is operating in
  Mode workerMode_;
  // Specialized Type this worker should be
  const Specialization& workerSpecialization_;
  // ID of the thread that this worker lives on
  std::thread::id threadID_;
  // If this worker should continue working
  volatile bool keepWorking_;
  // If this worker is currently working
  volatile bool isWorking_;

  /*
      Loop until the worker is told to stop
  */
  void DoWork();

  /*
      Take and complete a single job
  */
  void DoSingleJob();

  /*
      Aquire a job through some means

      Will take from its own queue first, or steal a job

      Returns some job if one was found, or nullptr otherwise
  */
  JobHandle GetAJob();
};
}
#endif
