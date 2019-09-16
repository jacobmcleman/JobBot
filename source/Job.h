/**************************************************************************
    Declaration of Job class with all function headers and code to ensure
    job has the correct size for cache coherency

    Author:
    Jake McLeman

    All content copyright 2017 DigiPen (USA) Corporation, all rights reserved.
***************************************************************************/
#ifndef _JOB_H
#define _JOB_H

#include <atomic>

namespace JobSystemTests
{
class JobSystemManagerTests;
}

namespace JobBot
{
enum struct JobType
{
  Tiny,
  Huge,
  IO,
  Graphics,
  Important,
  Misc,
  NumJobTypes,
  Null = -1
};

/*
    Forward declaration so that JobFunction typedefs can happen
*/
class Job;
class JobHandle;

/*
    Define JobFunction pointers to make declarations of Jobs much tidier
*/
typedef void (*JobFunctionPointer)(JobHandle);

struct JobFunction
{
  JobFunction(JobFunctionPointer func, JobType type = JobType::Misc);

  JobFunctionPointer function = nullptr;
  unsigned char flags         = 0;
};

class JobHandle
{
  private:
    class BlockingProxy 
    {
      public:
        BlockingProxy(JobHandle& handle);
        ~BlockingProxy();
      private:
        JobHandle& handle_;
    };

    class Properties
    {
      public:
        Properties(const JobHandle& handle, Properties& negative, bool negated): not(negative), handle_(handle), negated_(negated) {}

        bool Null() const;
        bool Finished() const;
        bool Running() const;
        bool Type(JobType type) const;

        Properties& not;

      private:
        const JobHandle& handle_;
        const bool negated_;
        bool negator(bool toNeg) const;
    };

  public:
    JobHandle(const JobHandle& aHandle);

    JobHandle& operator=(const JobHandle& aHandle);

    BlockingProxy Block();
    void BlockCompletion();
    void UnblockCompletion();

    Properties is;
    bool Run();
    bool SetCallback(JobFunction func);
    
    /*
      Retrive the associated data that is stored with this job.
      This data cannot be type checked when it is accessed, so
      when using make sure that the used type matches the expected type.

      Recommend using a struct for multiple arguments. This should
      be smaller than PADDING_BYTES (checked with a static assert).
    */
    template <typename T> T& GetData();

    friend class Job;
    friend class Manager;
    friend class JobRejected;
  private:
    JobHandle(Job* aJob): is(*this, isNot_, false), isNot_(*this, is, true), job(aJob) {}
    Properties isNot_;
    Job* job;
};

#pragma pack(push, 1)
class Job
{
public:
  /*
      Allocate memory for a job, giving it a function
  */
  static JobHandle Create(JobFunction& function);

  /*
      Allocate memory for a job, giving it a function and a parent
  */
  static JobHandle CreateChild(JobFunction& function, JobHandle parent);

  /*
      Allocate memory for a job, giving it a function and some data.
      Use a struct, array, pointer, or some other way of grouping
      things in memory that can cast back out on the far side to pass
      multiple pieces of data.

      2 Variants, for with/without parent jobs.

      These functions will cause a compilation failure if your data is
      too large to fit within a job itself.
  */
  template <typename T>
  static JobHandle Create(JobFunction& function, const T& data);
  template <typename T>
  static JobHandle CreateChild(JobFunction& function, const T& data, JobHandle parent);

  /*
      Execute this job
  */
  void Run();

  /*
      Check if this job has finished its work
  */
  bool IsFinished() const;

  /*
      Set a callback function to be executed after the completion
      of this job. This function will be run by the same worker
      that completed the job.

      func - the function to use as a callback
  */
  void SetCallback(JobFunction func);

  /*
      Does this job match the given type?
  */
  bool MatchesType(JobType type) const;

  /*
    Is this job currently in progress
  */
  bool InProgress() const;

  /*
      Associate some data with the job. This data cannot be type
      checked when it is accessed, so make sure that the expected
      types are well documented.

      Recommend using a struct for multiple arguments. This should
      be smaller than PADDING_BYTES (checked with a static assert).

      data - the data to put in the jobs storage space
  */
  template <typename T> void SetData(const T& data);

  /*
      Retrive the associated data that is stored with this job.
      This data cannot be type checked when it is accessed, so
      when using make sure that the used type matches the expected type.

      Recommend using a struct for multiple arguments. This should
      be smaller than PADDING_BYTES (checked with a static assert).
  */
  template <typename T> T& GetData();

  // Size that we want jobs to be
  static constexpr size_t TARGET_JOB_SIZE = 128;
  // Amount of data within a job
  static constexpr size_t PAYLOAD_SIZE =
      2 * sizeof(JobFunctionPointer) + sizeof(std::atomic_int) + sizeof(JobHandle) +
      sizeof(std::atomic_char) + sizeof(unsigned char);
  // Amount of bytes to add in order to reach target size
  static constexpr size_t PADDING_BYTES = TARGET_JOB_SIZE - PAYLOAD_SIZE;

  // Fail to compile if trying to add negative
  static_assert(PAYLOAD_SIZE < TARGET_JOB_SIZE,
                "Job size exceeds target job size");

#ifdef _DEBUG
  // Need access to private function to reset UnfinishedJobCount for testing
  // purposes.
  friend class JobSystemTests::JobSystemManagerTests;

  // Function to get the count of unfinished jobs
  static size_t GetUnfinishedJobCount();
#endif

  /*
      Set this job as able/unable to be finished

      Use if a thread needs to access some kind of data, or affect this
      job in some way (ex adding more children)

      Be sure to set any job previously marked as not completable
      back to being, completable, to avoid, any, infinite loops, which would
      be your fault at that point
  */
  void SetAllowCompletion(bool completable);

  /*
      Check if a job is currently allowed to be completed
  */
  bool GetCompletable() const;

private:
  // Function that contains the actual job behavior
  JobFunctionPointer jobFunc_;
  // Function that contains the callback (may be nullptr)
  JobFunctionPointer callbackFunc_;

  // Number of child jobs including this one that need to be completed before
  // this job is done
  std::atomic_int unfinishedJobs_;

  // Parent of this job
  JobHandle parent_;

  // Number of other parts of code that need this job to remain 'alive'
  std::atomic_char ghostJobCount_;

  // Information about this job
  unsigned char flags_;

  // Complete all steps to properly terminate a job
  void Finish();

  // Padding bytes
  unsigned char padding_[PADDING_BYTES];

#ifdef _DEBUG
  static std::atomic_size_t sJobsAdded_;
  static std::atomic_size_t sJobsCompleted_;

  static void ResetJobAddCompleteCounters();
#endif

  /*
      Constructors are deliberately private to prevent job allocation
      not through the specific functions for it. Allowing user-constructed jobs
      is error prone as they could go out of scope and their memory freed before
      job is executed, so all jobs live in preallocated space
      as a static member of this class
  */

  /*
      Default constructor to allow arrays of this
      type to be made for memory management
  */
  Job();

  /*
      Constructor to turn a job function into a job

      function - the function to run as this job
      parent - this job's parent (optional)
  */
  Job(JobFunction function, JobHandle parent = JobHandle(nullptr));

  /*
      Assignment operator for the job systems 'memory manager' to use
  */
  Job& operator=(const Job& job);

  // Amount of jobs for which memory should be preallocated
  // 2^16 is the maximum amount used in stress testing, but can easily be
  // increased
  static constexpr size_t scMaxJobAlloc_ = 1 << 16;
  // Bitmask to use instead of % for powers of 2
  static constexpr size_t scJobLoopBitMask_ = scMaxJobAlloc_ - 1;
  // Memory pool to allocate all jobs inside
  static Job sPreallocJobArray_[scMaxJobAlloc_];

  // Index of current place in pool used for allocation
  static std::atomic_size_t sCurJobIndex_;
};
#pragma pack(pop)

struct IOJobFunction : public JobFunction
{
  IOJobFunction(JobFunctionPointer func);
};

struct TinyJobFunction : public JobFunction
{
  TinyJobFunction(JobFunctionPointer func);
};

struct HugeJobFunction : public JobFunction
{
  HugeJobFunction(JobFunctionPointer func);
};

struct GraphicsJobFunction : public JobFunction
{
  GraphicsJobFunction(JobFunctionPointer func);
};

struct ImportantJobFunction : public JobFunction
{
  ImportantJobFunction(JobFunctionPointer func);
};

template <typename T>
inline JobHandle Job::Create(JobFunction& function, const T& data)
{
  JobHandle job = Create(function);
  job.job->SetData<T>(data);
  return job;
}

template <typename T>
inline JobHandle Job::CreateChild(JobFunction& function, const T& data, JobHandle parent)
{
  JobHandle job = CreateChild(function, parent);
  job.job->SetData<T>(data);
  return job;
}

template <typename T> inline void Job::SetData(const T& data)
{
  // Verify that the data will fit in the allocated space
  static_assert(sizeof(T) <= PADDING_BYTES,
                "Job data too large, recommend passing by pointer");

  // Put the data in the padding bytes
  *reinterpret_cast<T*>(padding_) = data;
}
template <typename T> inline T& Job::GetData()
{
  // Verify that the requested data is small enough to be in the padding space
  static_assert(sizeof(T) <= PADDING_BYTES,
                "Job data too large, recommend passing by pointer");

  // Get the data from the padding bytes
  return *reinterpret_cast<T*>(padding_);
}

template <typename T> inline T& JobHandle::GetData()
{
  return job->GetData<T>();
}

}

#define DECLARE_JOB(JobName)                                                   \
  void JobName##Func(JobBot::JobHandle job);                                        \
  JobBot::JobFunction JobName(JobName##Func);                                  \
  void JobName##Func(JobBot::JobHandle job)

#define DECLARE_IO_JOB(JobName)                                                \
  void JobName##Func(JobBot::JobHandle job);                                        \
  JobBot::IOJobFunction JobName(JobName##Func);                                \
  void JobName##Func(JobBot::JobHandle job)

#define DECLARE_TINY_JOB(JobName)                                              \
  void JobName##Func(JobBot::JobHandle job);                                        \
  JobBot::TinyJobFunction JobName(JobName##Func);                              \
  void JobName##Func(JobBot::JobHandle job)

#define DECLARE_HUGE_JOB(JobName)                                              \
  void JobName##Func(JobBot::JobHandle job);                                        \
  JobBot::HugeJobFunction JobName(JobName##Func);                              \
  void JobName##Func(JobBot::JobHandle job)

#define DECLARE_GRAPHICS_JOB(JobName)                                          \
  void JobName##Func(JobBot::JobHandle job);                                        \
  JobBot::GraphicsJobFunction JobName(JobName##Func);                          \
  void JobName##Func(JobBot::JobHandle job)

#define DECLARE_IMPORTANT_JOB(JobName)                                         \
  void JobName##Func(JobBot::JobHandle job);                                        \
  JobBot::ImportantJobFunction JobName(JobName##Func);                         \
  void JobName##Func(JobBot::JobHandle job)

#endif
