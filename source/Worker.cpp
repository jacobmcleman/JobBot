/**************************************************************************
    Contains implementation for Worker class and its JobQueue subclass, which
    handle job queues and stealing.

    Author:
    Jake McLeman
**************************************************************************/

#include <algorithm>
#ifdef _DEBUG
#include <assert.h>
#endif

#include "Job.h"
#include "JobExceptions.h"
#include "Manager.h"
#include "Worker.h"

namespace JobBot
{
Worker::Worker(Manager* aManager, Mode aMode,
               const Specialization& aSpecialization)
    : manager_(aManager), workerMode_(aMode),
      workerSpecialization_(aSpecialization),
      threadID_(std::this_thread::get_id()), keepWorking_(false),
      isWorking_(false)
{
}

Worker::Specialization Worker::Specialization::None = {
    {JobType::Huge, JobType::Graphics, JobType::Misc, JobType::IO,
     JobType::Tiny}};
Worker::Specialization Worker::Specialization::IO = {
    {JobType::IO, JobType::Huge, JobType::Misc, JobType::Graphics,
     JobType::Tiny}};
Worker::Specialization Worker::Specialization::Graphics = {
    {JobType::Graphics, JobType::Tiny, JobType::Misc, JobType::Null,
     JobType::Null}};
Worker::Specialization Worker::Specialization::RealTime = {
    {JobType::Tiny, JobType::Misc, JobType::Graphics, JobType::Null,
     JobType::Null}};

void Worker::WorkWhileWaitingFor(Job* aWaitJob)
{
  bool wasWorking = isWorking_;
  isWorking_      = true;

  aWaitJob->SetAllowCompletion(false);

  while (!aWaitJob->IsFinished())
  {
    DoSingleJob();
  }

  aWaitJob->SetAllowCompletion(true);

  isWorking_ = wasWorking;
}

void Worker::WorkWhileWaitingFor(std::atomic_bool& condition)
{
  bool wasWorking = isWorking_;
  isWorking_      = true;

  while (!condition)
  {
    DoSingleJob();
  }

  isWorking_ = wasWorking;
}

void Worker::Start()
{
  keepWorking_ = true;
  DoWork();
}

void Worker::Stop()
{
  keepWorking_ = false;

  while (isWorking_)
    ;
}

void Worker::StopAfterCurrentTask() { keepWorking_ = false; }

Worker::Mode Worker::GetMode() { return workerMode_; }

std::thread::id Worker::GetThreadID() const { return threadID_; }

bool Worker::IsWorking() const { return isWorking_; }

void Worker::DoWork()
{
  isWorking_ = true;

  while (keepWorking_)
  {
    DoSingleJob();
  }

  isWorking_ = false;
}

void Worker::DoSingleJob()
{
  static std::mutex waitMutex;
  Job* job = GetAJob();

#ifdef _DEBUG
  if (job != nullptr && job->GetUnfinishedJobCount() > 0)
#else
  if (job != nullptr)
#endif
  {
    job->Run();
  }
  else
  {
    // If no job was found by any method, be a good citizen and step aside
    // so that other processes on CPU can happen
    if (workerMode_ == Mode::Volunteer)
    {
      std::this_thread::yield();
    }
    else
    {
      std::unique_lock<std::mutex> uniqueWait(waitMutex);
      manager_->JobNotifier.wait(uniqueWait);
    }
  }
}

Job* Worker::GetAJob() { return manager_->RequestJob(workerSpecialization_); }
}
