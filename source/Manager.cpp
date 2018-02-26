/*****************************************************************************
    Declaration of manager class that handles creation of workers and
    assignment of jobs

    Author:
    Jake McLeman
******************************************************************************/

#include <assert.h>
#include <thread>

#include "Job.h"
#include "Manager.h"

namespace JobBot
{
Manager::Manager(size_t aNumWorkers)
    : workersWorking_(false),
      numWorkers_((aNumWorkers == 0) ? std::thread::hardware_concurrency()
                                       : aNumWorkers)
{
  workers_.reserve(numWorkers_);

  StartWorkers();
}

Manager::~Manager() { StopWorkers(); }

bool Manager::SubmitJob(Job* job)
{
  if (job == nullptr)
  {
    throw JobRejected(JobRejected::FailureType::NullJob, job);
  }

  if (job->MatchesType(JobType::Important))
    jobs[static_cast<size_t>(JobType::Important)].enqueue(job);
  else if (job->MatchesType(JobType::IO))
    jobs[static_cast<size_t>(JobType::IO)].enqueue(job);
  else if (job->MatchesType(JobType::Huge))
    jobs[static_cast<size_t>(JobType::Huge)].enqueue(job);
  else if (job->MatchesType(JobType::Graphics))
    jobs[static_cast<size_t>(JobType::Graphics)].enqueue(job);
  else if (job->MatchesType(JobType::Tiny))
    jobs[static_cast<size_t>(JobType::Tiny)].enqueue(job);
  else
    jobs[static_cast<size_t>(JobType::Misc)].enqueue(job);

  // Wake up any workers that went to sleep because there was no work to do
  // since there is now
  // Need to wake up all of them to avoid waking up a worker that can't do this
  // job
  JobNotifier.notify_all();

  return true;
}

Worker* Manager::GetWorkerByThreadID(std::thread::id id)
{
  /*
      TODO consider optimizing this linear search

      Potentially workers could be in a different data
      structure to speed this up?
  */
  for (Worker* worker : workers_)
  {
    if (worker->GetThreadID() == id)
    {
      return worker;
    }
  }

  return nullptr;
}

Worker* Manager::GetThisThreadsWorker()
{
  return GetWorkerByThreadID(std::this_thread::get_id());
}

Worker* Manager::GetRandomWorker()
{
  return workers_[std::rand() % workers_.size()];
}

Manager* Manager::GetInstance()
{
  static Manager man;
  return &man;
}

void Manager::RunJob(Job* job) { GetInstance()->SubmitJob(job); }

void Manager::WaitForJob(Job* job)
{
  GetInstance()->GetThisThreadsWorker()->WorkWhileWaitingFor(job);
}

Job* Manager::RequestJob(const Worker::Specialization& workerSpecialization)
{
  Job* job;

  if (TryGetJob(JobType::Important, job))
  {
    return job;
  }

  for (unsigned i = 0; i < static_cast<size_t>(JobType::NumJobTypes) - 1; ++i)
  {
    JobType toTry = workerSpecialization.priorities[i];
    if (toTry != JobType::Null && TryGetJob(toTry, job))
    {
      return job;
    }
  }

  return nullptr;
}

bool Manager::TryGetJob(JobType type, Job*& job)
{
  return jobs[static_cast<size_t>(type)].try_dequeue(job);
}

void Manager::StartNewWorker(Worker::Mode mode)
{
  // Possible specializations for primary workers
  // None is deliberately here twice to make it happen half the time
  constexpr Worker::Specialization* primarySpecs[] = {
      &Worker::Specialization::None, &Worker::Specialization::None,
      &Worker::Specialization::Graphics, &Worker::Specialization::IO};
  // Number of primary specializations in the above array
  constexpr size_t numPrimarySpecs =
      sizeof(primarySpecs) / sizeof(primarySpecs[0]);
  // Counter to use to circularly move through above array when chosing
  // specializations for new primary workers
  static unsigned primaryCounter = 0;

  const Worker::Specialization* specialization;
  if (mode == Worker::Mode::Volunteer)
  {
    if(numWorkers_ > 1)
    {
      // Volunteer workers are always marked as 'real time'
      specialization = &Worker::Specialization::RealTime;
    }
    else
    {
      //Running in single core mode the main thread has to take anything
      specialization = &Worker::Specialization::None;
    }
  }
  else
  {
    // Chose primary worker type based on above stuff
    specialization = primarySpecs[(primaryCounter++) % numPrimarySpecs];
  }

  workerMutex_.lock();
  workers_.push_back(new Worker(this, mode, *specialization));

  Worker* worker = workers_.back();
  workerMutex_.unlock();

  if (worker->GetMode() == Worker::Mode::Primary)
  {
    worker->Start();
  }
}

void Manager::StopWorkers()
{
  if (!workersWorking_) return;

  // Ask all workers to stop working
  for (Worker* worker : workers_)
  {
    worker->StopAfterCurrentTask();
  }

  // Let every sleeping worker know that now would be a great
  // time to wake up so they can see that I asked them to shut down
  JobNotifier.notify_all();

  // Wait for all workers to stop working
  for (Worker* worker : workers_)
  {
    worker->Stop();
  }

  // Free all workers seperately so that no one trys
  // to steal from a deleted worker
  for (Worker* worker : workers_)
  {
    delete worker;
  }

  // Join all threads
  while (!threads_.empty())
  {
    threads_.back().join();
    threads_.pop_back();
  }

  workersWorking_ = false;
}

void Manager::StartWorkers()
{
  if (workersWorking_) return;

  // Start the main thread worker in volunteer mode
  StartNewWorker(Worker::Mode::Volunteer);

  // For all other threads, workers exist in primary mode
  for (unsigned int i = 1; i < numWorkers_; ++i)
  {
    // Start a thread that will start a worker
    threads_.emplace_back([&]() { StartNewWorker(Worker::Mode::Primary); });
  }

  workersWorking_ = true;
}

void RunJob(Job* job) { JobBot::Manager::RunJob(job); }

void WaitForJob(Job* job) { JobBot::Manager::WaitForJob(job); }
}
