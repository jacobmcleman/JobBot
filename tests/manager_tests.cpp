/**************************************************************************
  Some short tests to test manager functionality

  Author:
  Jake McLeman
***************************************************************************/

#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <thread>

#include "Job.h"
#include "Manager.h"

using namespace JobBot;

#define UNUSED(thing) (void)thing

bool jobFunc1HasRun;
void JobFunc1(JobHandle job)
{
  UNUSED(job);
  jobFunc1HasRun = true;
}
TinyJobFunction Job1(JobFunc1);

bool jobFunc2HasRun;
void JobFunc2(JobHandle job)
{
  UNUSED(job);
  jobFunc2HasRun = true;
}
TinyJobFunction Job2(JobFunc2);

/*
  Test job function to see how the system handles a blocked worker

  Deliberately ask the scheduler to temporarily take this thread off the CPU
*/
bool sleepJobHasRun;
void SleepJobFunc(JobHandle job)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(job.GetData<int>()));
  sleepJobHasRun = true;
}
IOJobFunction SleepJob(SleepJobFunc);

/*
  Test function to deliberately upset the CPU with many slow floating point
  operations

  Keep this thread busy so that the scheduler can't take this job that takes
  significant
  time off the CPU
*/
bool floatsJobHasRun;
void FloatsJobFunc(JobHandle job)
{
  float base = job.GetData<float>();
  base       = base * base * base * 7 * base * base;
  std::pow(base, 2.7f);
  std::pow(base, 1.2f);
  std::pow(base, 10.432f);
  std::pow(base, 21.7f);
  std::pow(base, 1.32f);
  std::pow(base, 110.4432f);
  std::pow(base, 2.7f);
  std::pow(base, 1.2f);
  std::pow(base, 10.432f);
  std::pow(base, 21.7f);
  std::pow(base, 1.32f);
  std::pow(base, 110.4432f);
  floatsJobHasRun = true;
}
GraphicsJobFunction FloatsJob(FloatsJobFunc);

std::atomic_int splitterLeavesReached(0);
Manager* splitterMan;

void SplitterJobFunc(JobHandle job);
JobFunction SplitterJob(SplitterJobFunc);

DECLARE_HUGE_JOB(TolBoiJob)
{
  float base = job.GetData<float>();
  base       = base * base * base * 7 * base * base;
  std::pow(base, 2.7f);
  std::pow(base, 1.2f);
  std::pow(base, 10.432f);
  std::pow(base, 21.7f);
  std::pow(base, 1.32f);
  std::pow(base, 110.4432f);
  std::pow(base, 2.7f);
  std::pow(base, 1.2f);
  std::pow(base, 10.432f);
  std::pow(base, 21.7f);
  std::pow(base, 1.32f);
  std::pow(base, 110.4432f);

  std::pow(base, 2.7f);
  std::pow(base, 1.2f);
  std::pow(base, 10.432f);
  std::pow(base, 21.7f);
  std::pow(base, 1.32f);
  std::pow(base, 110.4432f);
  std::pow(base, 2.7f);
  std::pow(base, 1.2f);
  std::pow(base, 10.432f);
  std::pow(base, 21.7f);
  std::pow(base, 1.32f);
  std::pow(base, 110.4432f);
  floatsJobHasRun = true;
}

void SplitterJobFunc(JobHandle job)
{
  int splitLevelsLeft = job.GetData<int>();
  if (splitLevelsLeft == 0)
  {
    ++splitterLeavesReached;
  }
  else
  {
    auto lock = job.Block();
    splitterMan->SubmitJob(
        Job::CreateChild<int>(SplitterJob, splitLevelsLeft - 1, job));
    splitterMan->SubmitJob(
        Job::CreateChild<int>(SplitterJob, splitLevelsLeft - 1, job));
  }
}

TEST(ManagerTests, GetThisThreadsWorker)
{
  Manager man(1);

  std::thread::id workerID = man.GetThisThreadsWorker()->GetThreadID();
  std::thread::id thisID   = std::this_thread::get_id();
  EXPECT_TRUE(workerID == thisID)
      << "Single worker thread is not on main thread";
}

TEST(ManagerTests, SingleThreadFewJobs)
{
  Manager man(1);

  JobHandle job1 = Job::Create(Job1);
  JobHandle job2 = Job::CreateChild(Job2, job1);
  JobHandle job3 = Job::CreateChild(Job2, 2, job2);
  JobHandle job4 = Job::CreateChild(FloatsJob, 0.1f, job1);
  JobHandle job5 = Job::CreateChild(FloatsJob, 2.4f, job1);

  man.SubmitJob(job1);
  man.SubmitJob(job2);
  man.SubmitJob(job3);
  man.SubmitJob(job4);
  man.SubmitJob(job5);

  EXPECT_FALSE(job1.is.Finished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job2.is.Finished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job3.is.Finished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job4.is.Finished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job5.is.Finished()) << "Job has been prematurely executed";

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(job1);

  EXPECT_TRUE(job1.is.Finished()) << "Job has not been completed";
  EXPECT_TRUE(job2.is.Finished()) << "Job has not been completed";
  EXPECT_TRUE(job3.is.Finished()) << "Job has not been completed";
  EXPECT_TRUE(job4.is.Finished()) << "Job has not been completed";
  EXPECT_TRUE(job5.is.Finished()) << "Job has not been completed";

  EXPECT_TRUE(jobFunc1HasRun) << "Job has not been executed";
  EXPECT_TRUE(jobFunc2HasRun) << "Job has not been executed";
  EXPECT_TRUE(floatsJobHasRun) << "Job has not been executed";

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount())
      << "Some jobs were not finished";
#endif
}

TEST(ManagerTests, SingleThreadManyJobs)
{
  constexpr size_t jobsToMake = 1024;
  std::vector<JobHandle> jobs;

  Manager man(1);

  jobs.push_back(Job::Create(Job1));
  JobHandle parentJob = jobs[0];
  man.SubmitJob(parentJob);

  for (unsigned i = 1; i < jobsToMake; ++i)
  {
    jobs.push_back(Job::CreateChild<float>(
        FloatsJob, (((float)std::rand()) / ((float)std::rand() + 1)),
        parentJob));
    man.SubmitJob(jobs[i]);

    EXPECT_FALSE(jobs[i].is.Finished()) << "Job has been prematurely executed";
  }

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(parentJob);

  for (unsigned i = 0; i < jobsToMake; ++i)
  {
    EXPECT_TRUE(jobs[i].is.Finished()) << "Job has not been completed";
  }

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount())
      << "Some jobs were not finished";
#endif
}

/*
  This test will sleep for 3ms to make sure the threads have enough time to
  crash if they are going to
*/
TEST(ManagerTests, MultiThreadStartStop)
{
  Manager man(4);

  std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

TEST(ManagerTests, MultiThreadFewJobs)
{
  constexpr size_t workersToUse = 4;
  Manager man(workersToUse);

  JobHandle job1 = Job::Create(Job1);

  job1.BlockCompletion();
  JobHandle job2 = Job::CreateChild(Job2, job1);
  JobHandle job3 = Job::CreateChild(SleepJob, 2, job1);
  JobHandle job4 = Job::CreateChild(FloatsJob, 0.1f, job1);
  JobHandle job5 = Job::CreateChild(FloatsJob, 2.4f, job1);
  job1.UnblockCompletion();

  man.SubmitJob(job1);
  man.SubmitJob(job2);
  man.SubmitJob(job3);
  man.SubmitJob(job4);
  man.SubmitJob(job5);

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(job1);

  EXPECT_TRUE(job1.is.Finished()) << "Job1 has not been completed";
  EXPECT_TRUE(job2.is.Finished()) << "Job2 has not been completed";
  EXPECT_TRUE(job3.is.Finished()) << "Job3 has not been completed";
  EXPECT_TRUE(job4.is.Finished()) << "Job4 has not been completed";
  EXPECT_TRUE(job5.is.Finished()) << "Job5 has not been completed";

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount())
      << "Some jobs were not finished";
#endif
}

TEST(ManagerTests, MultiThreadManyJobs)
{
  constexpr size_t workersToUse = 4;
  constexpr size_t jobsToMake   = 2048 * 1;
  std::vector<JobHandle> jobs;

  Manager man(workersToUse);

  jobs.push_back(Job::Create(Job1));
  JobHandle parentJob = jobs[0];
  parentJob.BlockCompletion();
  man.SubmitJob(parentJob);

  for (unsigned i = 1; i < jobsToMake; ++i)
  {
    jobs[i] = Job::CreateChild<float>(
        FloatsJob, (((float)std::rand()) / ((float)std::rand() + 1)),
        parentJob);
    man.SubmitJob(jobs[i]);
  }
  parentJob.UnblockCompletion();

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(parentJob);

  for (unsigned i = 0; i < jobsToMake; ++i)
  {
    EXPECT_TRUE(jobs[i].is.Finished()) << "Job has not been completed";
  }

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount())
      << "Some jobs were not finished";
#endif
}

// Current time is roughly 300 ms
// If it takes much longer, something is wrong
TEST(ManagerTests, StressTest)
{
  constexpr size_t workersToUse = 8;
  constexpr size_t jobsToMake   = 1 << 16;
  std::vector<JobHandle> jobs;

  Manager man(workersToUse);

  jobs.push_back(Job::Create(Job1));
  JobHandle parentJob = jobs[0];
  parentJob.BlockCompletion();

  man.SubmitJob(parentJob);

  for (unsigned i = 1; i < jobsToMake; ++i)
  {
    jobs[i] = Job::CreateChild<float>(
        FloatsJob, (((float)std::rand()) / ((float)std::rand() + 1)),
        parentJob);

    bool failed = false;

    // There I fixed it
    do
    {
      try
      {
        man.SubmitJob(jobs[i]);
        failed = false;
      }
      catch (const JobRejected& e)
      {
        (void)e;
        std::this_thread::yield();
        failed = true;
      }
    } while (failed);
  }
  parentJob.UnblockCompletion();

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(parentJob);

  for (unsigned i = 0; i < jobsToMake; ++i)
  {
    EXPECT_TRUE(jobs[i].is.Finished()) << "Job has not been completed";
  }

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount())
      << "Some jobs were not finished";
#endif
}

/*
  "Tribble Test" Each job spawns 2 more jobs down to the maximum depth
*/
TEST(ManagerTests, SingleThreadSplittingJobs)
{
  int maxDepth = 4;

  Manager man(1);
  splitterMan = &man;

  JobHandle splitter = Job::Create<int>(SplitterJob, maxDepth);
  man.SubmitJob(splitter);

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(splitter);

  EXPECT_EQ(1 << (maxDepth), splitterLeavesReached.load());
#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount());
#endif
  splitterMan = nullptr;

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount())
      << "Some jobs were not finished";
#endif
}

TEST(ManagerTests, SingleThreadWillTakeAnyJob)
{
  JobHandle sleepyJob = Job::Create<int>(SleepJob, 1);
  JobHandle otherJob  = Job::Create(Job1);
  Manager man(1);
  man.SubmitJob(otherJob);
  man.SubmitJob(sleepyJob);

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(otherJob);
  man.GetThisThreadsWorker()->WorkWhileWaitingFor(sleepyJob);

  EXPECT_TRUE(sleepyJob.is.Finished());
  EXPECT_TRUE(otherJob.is.Finished());
}
