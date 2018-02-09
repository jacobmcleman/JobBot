/**************************************************************************
  Some short tests to test manager functionality

  Author:
  Jake McLeman
***************************************************************************/

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cmath>

#include "Job.h"
#include "Manager.h"

using namespace JobBot;

bool jobFunc1HasRun;
void JobFunc1(Job* job) { jobFunc1HasRun = true; }
TinyJobFunction Job1(JobFunc1);

bool jobFunc2HasRun;
void JobFunc2(Job* job) { jobFunc2HasRun = true; }
TinyJobFunction Job2(JobFunc2);

/*
  Test job function to see how the system handles a blocked worker

  Deliberately ask the scheduler to temporarily take this thread off the CPU
*/
bool sleepJobHasRun;
void SleepJobFunc(Job* job)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(job->GetData<int>()));
  sleepJobHasRun = true;
}
IOJobFunction SleepJob(SleepJobFunc);

/*
  Test function to deliberately upset the CPU with many slow floating point operations

  Keep this thread busy so that the scheduler can't take this job that takes significant
  time off the CPU
*/
bool floatsJobHasRun;
void FloatsJobFunc(Job* job)
{
  float base = job->GetData<float>();
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

void SplitterJobFunc(Job* job);
JobFunction SplitterJob(SplitterJobFunc);

DECLARE_HUGE_JOB(TolBoiJob)
{
  float base = job->GetData<float>();
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

void SplitterJobFunc(Job* job)
{
  int splitLevelsLeft = job->GetData<int>();
  if (splitLevelsLeft == 0)
  {
    ++splitterLeavesReached;
  }
  else
  {
    job->SetAllowCompletion(false);
    splitterMan->SubmitJob(Job::CreateChild<int>(SplitterJob, splitLevelsLeft - 1, job));
    splitterMan->SubmitJob(Job::CreateChild<int>(SplitterJob, splitLevelsLeft - 1, job));
    job->SetAllowCompletion(true);
  }
}

TEST(ManagerTests, GetThisThreadsWorker)
{
  Manager man(1);

  std::thread::id workerID = man.GetThisThreadsWorker()->GetThreadID();
  std::thread::id thisID   = std::this_thread::get_id();
  EXPECT_TRUE(workerID == thisID) << "Single worker thread is not on main thread";
}

TEST(ManagerTests, SingleThreadFewJobs)
{
  Manager man(1);

  Job* job1 = Job::Create(Job1);
  Job* job2 = Job::CreateChild(Job2, job1);
  Job* job3 = Job::CreateChild(Job2, 2, job2);
  Job* job4 = Job::CreateChild(FloatsJob, 0.1f, job1);
  Job* job5 = Job::CreateChild(FloatsJob, 2.4f, job1);

  man.SubmitJob(job1);
  man.SubmitJob(job2);
  man.SubmitJob(job3);
  man.SubmitJob(job4);
  man.SubmitJob(job5);

  EXPECT_FALSE(job1->IsFinished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job2->IsFinished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job3->IsFinished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job4->IsFinished()) << "Job has been prematurely executed";
  EXPECT_FALSE(job5->IsFinished()) << "Job has been prematurely executed";

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(job1);

  EXPECT_TRUE(job1->IsFinished()) << "Job has not been completed";
  EXPECT_TRUE(job2->IsFinished()) << "Job has not been completed";
  EXPECT_TRUE(job3->IsFinished()) << "Job has not been completed";
  EXPECT_TRUE(job4->IsFinished()) << "Job has not been completed";
  EXPECT_TRUE(job5->IsFinished()) << "Job has not been completed";

  EXPECT_TRUE(jobFunc1HasRun) << "Job has not been executed";
  EXPECT_TRUE(jobFunc2HasRun) << "Job has not been executed";
  EXPECT_TRUE(floatsJobHasRun) << "Job has not been executed";

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount()) << "Some jobs were not finished";
#endif
}

TEST(ManagerTests, SingleThreadManyJobs)
{
  constexpr size_t jobsToMake = 1024;
  Job* jobs[jobsToMake];

  Manager man(1);

  jobs[0]        = Job::Create(Job1);
  Job* parentJob = jobs[0];
  man.SubmitJob(parentJob);

  for (int i = 1; i < jobsToMake; ++i)
  {
    jobs[i] = Job::CreateChild<float>(FloatsJob, (((float)std::rand()) / ((float)std::rand() + 1)), parentJob);
    man.SubmitJob(jobs[i]);

    EXPECT_FALSE(jobs[i]->IsFinished()) << "Job has been prematurely executed";
  }

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(parentJob);

  for (int i = 0; i < jobsToMake; ++i)
  {
    EXPECT_TRUE(jobs[i]->IsFinished()) << "Job has not been completed";
  }

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount()) << "Some jobs were not finished";
#endif
}

TEST(ManagerTests, SubmitNullJob)
{
  Manager man(1);

  bool exceptionThrown = false;

  try
  {
    man.SubmitJob(nullptr);
  }
  catch (JobRejected& e)
  {
    exceptionThrown = true;
  }

  EXPECT_TRUE(exceptionThrown) << "Exception was not thrown";
}

/*
  This test will sleep for 3ms to make sure the threads have enough time to crash if they are going to
*/
TEST(ManagerTests, MultiThreadStartStop)
{
  Manager man(4);

  std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

TEST(ManagerTests, MultiThreadFewJobs)
{
  Manager man(8);

  Job* job1 = Job::Create(Job1);
  job1->SetAllowCompletion(false);
  Job* job2 = Job::CreateChild(Job2, job1);
  Job* job3 = Job::CreateChild(SleepJob, 2, job1);
  Job* job4 = Job::CreateChild(FloatsJob, 0.1f, job1);
  Job* job5 = Job::CreateChild(FloatsJob, 2.4f, job1);
  job1->SetAllowCompletion(true);

  man.SubmitJob(job1);
  man.SubmitJob(job2);
  man.SubmitJob(job3);
  man.SubmitJob(job4);
  man.SubmitJob(job5);

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(job1);

  EXPECT_TRUE(job1->IsFinished()) << "Job1 has not been completed";
  EXPECT_TRUE(job2->IsFinished()) << "Job2 has not been completed";
  EXPECT_TRUE(job3->IsFinished()) << "Job3 has not been completed";
  EXPECT_TRUE(job4->IsFinished()) << "Job4 has not been completed";
  EXPECT_TRUE(job5->IsFinished()) << "Job5 has not been completed";

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount()) << "Some jobs were not finished";
#endif
}

TEST(ManagerTests, MultiThreadManyJobs)
{
  constexpr size_t workersToUse = 4;
  constexpr size_t jobsToMake   = 2048 * 1;
  Job* jobs[jobsToMake];

  Manager man(workersToUse);

  jobs[0]        = Job::Create(Job1);
  Job* parentJob = jobs[0];
  parentJob->SetAllowCompletion(false);
  man.SubmitJob(parentJob);

  for (int i = 1; i < jobsToMake; ++i)
  {
    jobs[i] = Job::CreateChild<float>(FloatsJob, (((float)std::rand()) / ((float)std::rand() + 1)), parentJob);
    man.SubmitJob(jobs[i]);
  }
  parentJob->SetAllowCompletion(true);

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(parentJob);

  for (int i = 0; i < jobsToMake; ++i)
  {
    EXPECT_TRUE(jobs[i]->IsFinished()) << "Job has not been completed";
  }

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount()) << "Some jobs were not finished";
#endif
}

// Current time is roughly 300 ms
// If it takes much longer, something is wrong
TEST(ManagerTests, StressTest)
{
  constexpr size_t workersToUse = 8;
  constexpr size_t jobsToMake   = 1 << 16;
  Job* jobs[jobsToMake];

  Manager man(workersToUse);

  jobs[0]        = Job::Create(Job1);
  Job* parentJob = jobs[0];
  parentJob->SetAllowCompletion(false);

  man.SubmitJob(parentJob);

  for (int i = 1; i < jobsToMake; ++i)
  {
    jobs[i] = Job::CreateChild<float>(FloatsJob, (((float)std::rand()) / ((float)std::rand() + 1)), parentJob);

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
  parentJob->SetAllowCompletion(true);

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(parentJob);

  for (int i = 0; i < jobsToMake; ++i)
  {
    EXPECT_TRUE(jobs[i]->IsFinished()) << "Job has not been completed";
  }

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount()) << "Some jobs were not finished";
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

  Job* splitter = Job::Create<int>(SplitterJob, maxDepth);
  man.SubmitJob(splitter);

  man.GetThisThreadsWorker()->WorkWhileWaitingFor(splitter);

  EXPECT_EQ(1 << (maxDepth), splitterLeavesReached.load());
#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount());
#endif
  splitterMan = nullptr;

#ifdef _DEBUG
  EXPECT_EQ((size_t)0, Job::GetUnfinishedJobCount()) << "Some jobs were not finished";
#endif
}

TEST(ManagerTests, MainThreadWontTakeIOJob)
{
  Job* sleepyJob = Job::Create<int>(SleepJob, 1);
  Job* otherJob  = Job::Create(Job1);
  Manager man(1);
  man.SubmitJob(otherJob);
  man.SubmitJob(sleepyJob);

  man.WaitForJob(otherJob);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_FALSE(sleepyJob->IsFinished()) << "Main Thread did an IO job. This is a bad.";
}

TEST(ManagerTests, MainThreadWontTakeBeefyJob)
{
  Job* otherJob = Job::Create(Job1);
  Job* tolBoi   = Job::Create<int>(TolBoiJob, 1);

  Manager man(1);
  man.SubmitJob(tolBoi);
  man.SubmitJob(otherJob);

  man.WaitForJob(otherJob);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_FALSE(tolBoi->IsFinished()) << "Main Thread did an big job. This is a bad.";
}
