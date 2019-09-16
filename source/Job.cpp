/**************************************************************************
    Contains implementation for job management, parenting, and callback code

    Author:
    Jake McLeman
***************************************************************************/

#include <assert.h>
#include <cstring> //memcpy
#include <mutex>

#include "Job.h"

namespace JobBot
{
// Initialize static members of job class
std::atomic_size_t Job::sCurJobIndex_(0);
Job Job::sPreallocJobArray_[scMaxJobAlloc_] = {};

#ifdef _DEBUG
std::atomic_size_t Job::sJobsAdded_     = 0;
std::atomic_size_t Job::sJobsCompleted_ = 0;
#endif

constexpr unsigned char JOB_FLAG_MASK_TINY =
    1 << static_cast<unsigned>(JobType::Tiny);
constexpr unsigned char JOB_FLAG_MASK_HUGE =
    1 << static_cast<unsigned>(JobType::Huge);
constexpr unsigned char JOB_FLAG_MASK_IO =
    1 << static_cast<unsigned>(JobType::IO);
constexpr unsigned char JOB_FLAG_MASK_GRAPHICS =
    1 << static_cast<unsigned>(JobType::Graphics);
constexpr unsigned char JOB_FLAG_MASK_IMPORTANT =
    1 << static_cast<unsigned>(JobType::Important);

constexpr unsigned char JOB_FLAG_MASK_STATUS_IN_PROGRESS =
    JOB_FLAG_MASK_IMPORTANT << 1;
constexpr unsigned char JOB_FLAG_MASK_STATUS_CANCELLED =
    JOB_FLAG_MASK_STATUS_IN_PROGRESS << 1;

JobHandle::BlockingProxy::BlockingProxy(JobHandle& aHandle): handle_(aHandle) 
{
  handle_.BlockCompletion();
}

JobHandle::BlockingProxy::~BlockingProxy() 
{
  handle_.UnblockCompletion();
}

JobHandle::JobHandle(const JobHandle& aHandle): is(*this, isNot_, false), isNot_(*this, is, true), job(aHandle.job) {}

JobHandle& JobHandle::operator=(const JobHandle& aHandle) {job = aHandle.job; return *this; }

bool JobHandle::Properties::negator(bool toNeg) const { return negated_ ? !toNeg : toNeg; }
bool JobHandle::Properties::Null() const { return negator(handle_.job == nullptr); }
bool JobHandle::Properties::Finished() const { return negator(handle_.job->IsFinished()); }
bool JobHandle::Properties::Running() const { return negator(handle_.job->InProgress()); }
bool JobHandle::Properties::Type(JobType type) const { return negator(handle_.job->MatchesType(type)); }

void JobHandle::BlockCompletion() { job->SetAllowCompletion(false); }
void JobHandle::UnblockCompletion() { job->SetAllowCompletion(true); }
JobHandle::BlockingProxy JobHandle::Block() { return BlockingProxy(*this); }

bool JobHandle::SetCallback(JobFunction func) 
{ 
  if (!is.Null() && !is.Finished())
  {
    job->SetCallback(func);
    return true;
  }
  else
  {
    return false;
  }
}

bool JobHandle::Run() 
{
  if (!is.Null() && !is.Finished())
  {
    job->Run();
    return true;
  }
  else
  {
    return false;
  }
}

Job::Job()
    : jobFunc_(nullptr), callbackFunc_(nullptr), unfinishedJobs_(-1),
      parent_(nullptr), ghostJobCount_(0), flags_(0)
{
}

Job::Job(JobFunction function, JobHandle parent)
    : jobFunc_(function.function), callbackFunc_(nullptr), unfinishedJobs_(1),
      parent_(parent), ghostJobCount_(0),
      flags_(
          function.flags &
          ~(JOB_FLAG_MASK_STATUS_IN_PROGRESS | JOB_FLAG_MASK_STATUS_CANCELLED))
{
  // If there is a parent, it now has one more job that must finish before
  // parent is done
  if (parent_.job != nullptr)
  {
    ++(parent_.job->unfinishedJobs_);
  }

#ifdef _DEBUG
  Job::sJobsAdded_++;
#endif
}

Job& Job::operator=(const Job& job)
{
  jobFunc_      = job.jobFunc_;
  callbackFunc_ = job.callbackFunc_;
  unfinishedJobs_.store(job.unfinishedJobs_.load());
  parent_ = job.parent_;
  ghostJobCount_.store(job.ghostJobCount_.load());
  std::memcpy(padding_, job.padding_, PADDING_BYTES);
  return *this;
}

JobHandle Job::Create(JobFunction& function)
{
  return CreateChild(function, nullptr);
}

JobHandle Job::CreateChild(JobFunction& function, JobHandle parent)
{
  Job* nextJob = nullptr;

  do
  {
    // Grab the index and move the index forward atomically before trying
    // to grab a job to avoid thread conflicts
    size_t myJobIndex = sCurJobIndex_++;

    // Grab the next job from the pool
    // Use & as a bitmask instead of modulo since the array is a power of 2 in
    // size
    nextJob = &(sPreallocJobArray_[myJobIndex & scJobLoopBitMask_]);

    // Make sure this job is not currently in use
    // Default constructed jobs (that occupy the entire array initially)
    // will automagically meet this condition
  } while (nextJob->ghostJobCount_ != 0 || nextJob->unfinishedJobs_ > -1);

  // Now that job has been found, make sure its set as completable by default
  nextJob->ghostJobCount_ = 0;

  *nextJob = Job(function, parent);

  // Ensure jobs cannot be initialized with an in-progress flag
  nextJob->flags_ = function.flags;

  return JobHandle{nextJob};
}

void Job::Run()
{
  // If there is a job function
  if (jobFunc_ != nullptr)
  {
    // Mark this job as in progress
    flags_ |= JOB_FLAG_MASK_STATUS_IN_PROGRESS;

    // Run the job function
    jobFunc_(this);

    // Complete the job
    Finish();
  }
}

bool Job::IsFinished() const
{
  // Job is finished when there are no unfinished jobs
  return unfinishedJobs_ <= 0;
}

void Job::SetCallback(JobFunction func) { callbackFunc_ = func.function; }

bool Job::MatchesType(JobType type) const
{
  // Misc means matches no other type
  if (type == JobType::Misc &&
      (flags_ & ((JOB_FLAG_MASK_IMPORTANT << 1) - 1)) == 0)
    return true;
  else
  {
    // Otherwise flag bits follow predictable bit pattern
    return (flags_ & (1 << static_cast<unsigned>(type)));
  }
}

bool Job::InProgress() const
{
  return flags_ & JOB_FLAG_MASK_STATUS_IN_PROGRESS;
}

void Job::Finish()
{
  char cachedGhostJobs = ghostJobCount_.load();
  --unfinishedJobs_;

  if (unfinishedJobs_ == 0 && cachedGhostJobs == 0)
  {
    // Run the callback to allow user to clean up if it exists
    if (callbackFunc_ != nullptr)
    {
      callbackFunc_(this);
    }

    if (parent_.job != nullptr)
    {
      parent_.job->Finish();
    }

    flags_ &= ~JOB_FLAG_MASK_STATUS_IN_PROGRESS;

    // Decrement unfinishedJobs_ once more to bring it to -1
    // so that allocator can see this jobs is completely finished
    --unfinishedJobs_;
#ifdef _DEBUG
    ++sJobsCompleted_;

    assert(unfinishedJobs_ == -1);
    assert(jobFunc_ != nullptr);
#endif
  }
}

#ifdef _DEBUG
void Job::ResetJobAddCompleteCounters()
{
  sJobsAdded_     = 0;
  sJobsCompleted_ = 0;
}

size_t Job::GetUnfinishedJobCount() { return sJobsAdded_ - sJobsCompleted_; }
#endif

void Job::SetAllowCompletion(bool aCompletable)
{
  // If a job is not completable, there is some other 'Job' that
  // this is dependent on that is not complete, so count that as
  // an unfinished child
  if (aCompletable == false)
  {
    ghostJobCount_++;
  }

  if (aCompletable == true)
  {
    ghostJobCount_--;

    // Finish call below is going to decrement unfinished jobs
    // So start off by incrementing
    unfinishedJobs_++;

    // A completion lock being removed is the same
    // as a child job being finished
    Finish();
  }
}

bool Job::GetCompletable() const { return ghostJobCount_; }

JobFunction::JobFunction(JobFunctionPointer func, JobType type)
    : function(func), flags(0)
{
  unsigned char flag = 0;
  if (type == JobType::Tiny) flag |= JOB_FLAG_MASK_TINY;
  if (type == JobType::Huge) flag |= JOB_FLAG_MASK_HUGE;
  if (type == JobType::IO) flag |= JOB_FLAG_MASK_IO;
  if (type == JobType::Graphics) flag |= JOB_FLAG_MASK_GRAPHICS;
  if (type == JobType::Important) flag |= JOB_FLAG_MASK_IMPORTANT;

  flags = flag & ~JOB_FLAG_MASK_STATUS_IN_PROGRESS;
}

IOJobFunction::IOJobFunction(JobFunctionPointer func)
    : JobFunction(func, JobType::IO)
{
}

TinyJobFunction::TinyJobFunction(JobFunctionPointer func)
    : JobFunction(func, JobType::Tiny)
{
}

HugeJobFunction::HugeJobFunction(JobFunctionPointer func)
    : JobFunction(func, JobType::Huge)
{
}

GraphicsJobFunction::GraphicsJobFunction(JobFunctionPointer func)
    : JobFunction(func, JobType::Graphics)
{
}
ImportantJobFunction::ImportantJobFunction(JobFunctionPointer func)
    : JobFunction(func, JobType::Important)
{
}
}
