/**************************************************************************
  Some short tests to test basic job functionality

  Author:
  Jake McLeman
***************************************************************************/

#include <gtest/gtest.h>

#include "Job.h"

#define UNUSED(thing) (void)thing

using namespace JobBot;

bool testFunc1HasRun;
void TestJobFunc1(JobHandle job)
{
  UNUSED(job);
  testFunc1HasRun = true;
}
TinyJobFunction TestJob1(TestJobFunc1);

bool testFunc2HasRun;
void TestJobFunc2(JobHandle job)
{
  UNUSED(job);
  testFunc2HasRun = true;
}
HugeJobFunction TestJob2(TestJobFunc2);

bool testFunc3GotData;
void TestJobFunc3(JobHandle job) { testFunc3GotData = (job.GetData<int>() == 4); }
IOJobFunction TestJob3(TestJobFunc3);

bool testFunc4GotData;
void TestJobFunc4(JobHandle job)
{
  testFunc4GotData = (job.GetData<float>() == 25.12f);
}
GraphicsJobFunction TestJob4(TestJobFunc4);

TEST(JobTests, SizeVerification)
{
  ASSERT_EQ((size_t)Job::TARGET_JOB_SIZE, sizeof(Job))
      << "Job was incorrect size";
}

TEST(JobTests, Create)
{
  JobHandle job = Job::Create(TestJob1);

  EXPECT_FALSE(job.is.Finished()) << "Job has not been created correctly";
  EXPECT_FALSE(job.is.Running()) << "Job has not been created correctly";
}

TEST(JobTests, RunJob)
{
  testFunc1HasRun = false;
  JobHandle job        = Job::Create(TestJob1);

  EXPECT_FALSE(testFunc1HasRun) << "Job has been prematurely executed";
  EXPECT_FALSE(job.is.Finished())
      << "Job is marked as finished before it has run";

  EXPECT_TRUE(job.Run()) << "Job was unable to run";

  EXPECT_TRUE(testFunc1HasRun)
      << "Job has been run but has not executed job code";
  EXPECT_TRUE(job.is.Finished())
      << "Job has been run but is not marked as finished";
  EXPECT_FALSE(job.is.Running())
      << "Job is finished but still marked as in progress";
}

TEST(JobTests, Parent)
{
  testFunc1HasRun = false;
  testFunc2HasRun = false;

  // Create 2 jobs with job2 as a child of job1
  JobHandle job1 = Job::Create(TestJob1);
  JobHandle job2 = Job::CreateChild(TestJob2, job1);

  // Make sure that nothing has prematurely fired
  EXPECT_FALSE(testFunc1HasRun) << "Job1 has been prematurely executed";
  EXPECT_FALSE(job1.is.Finished())
      << "Job1 is marked as finished before it has run";
  EXPECT_FALSE(testFunc2HasRun) << "Job2 has been prematurely executed";
  EXPECT_FALSE(job2.is.Finished())
      << "Job2 is marked as finished before it has run";

  // Run job1 (the parent)
  EXPECT_TRUE(job1.Run()) << "Job was unable to run";

  // Job 1 has now run, but should not be marked done because it has a child
  // that has not finished
  EXPECT_TRUE(testFunc1HasRun) << "Job1 has not run correctly";
  EXPECT_FALSE(job1.is.Finished())
      << "Job1 is marked as finished before all of its children have finished";
  EXPECT_FALSE(testFunc2HasRun) << "Job2 has been prematurely executed";
  EXPECT_FALSE(job2.is.Finished())
      << "Job2 is marked as finished before it has run";

  // Run job2 (the child)
  EXPECT_TRUE(job2.Run()) << "Job was unable to run";

  // Make sure all jobs are now correctly marked as completed
  EXPECT_TRUE(testFunc1HasRun) << "Job1 has not run correctly";
  EXPECT_TRUE(job1.is.Finished())
      << "Job1 is not marked as finished even though all child jobs are done";
  EXPECT_TRUE(testFunc2HasRun) << "Job2 has not run correctly";
  EXPECT_TRUE(job2.is.Finished())
      << "Job2 has been run but is not marked as finished";
}

TEST(JobTests, Callback)
{
  testFunc1HasRun = false;
  testFunc2HasRun = false;

  // Create job1 with a callback function
  JobHandle job = Job::Create(TestJob1);
  EXPECT_TRUE(job.SetCallback(TestJob2)) << "Unable to set callback on job";

  EXPECT_FALSE(testFunc1HasRun) << "Job has been prematurely executed";
  EXPECT_FALSE(job.is.Finished())
      << "Job is marked as finished before it has run";
  EXPECT_FALSE(testFunc2HasRun) << "Callback has been prematurely executed";

  EXPECT_TRUE(job.Run()) << "Job was unable to run";

  EXPECT_TRUE(testFunc1HasRun)
      << "Job has been run but has not executed job code";
  EXPECT_TRUE(job.is.Finished())
      << "Job has been run but is not marked as finished";
  EXPECT_TRUE(testFunc2HasRun)
      << "Job has been run but has not executed callback code";
}

TEST(JobTests, Data1)
{
  testFunc3GotData = false;

  JobHandle job = Job::Create(TestJob3, 4);

  EXPECT_FALSE(testFunc3GotData)
      << "Function somehow already got the data even though it hasn't run yet";
  EXPECT_FALSE(job.is.Finished())
      << "Job is marked as finished before it has run";

  EXPECT_TRUE(job.Run()) << "Job was unable to run";

  EXPECT_TRUE(testFunc3GotData)
      << "Function did not correctly recieve the data";
  EXPECT_TRUE(job.is.Finished()) << "Job is not marked as finished";
}

TEST(JobTests, Data2)
{
  testFunc4GotData = false;

  JobHandle job = Job::Create(TestJob4, 25.12f);

  EXPECT_FALSE(testFunc4GotData)
      << "Function somehow already got the data even though it hasn't run yet";
  EXPECT_FALSE(job.is.Finished())
      << "Job is marked as finished before it has run";

  EXPECT_TRUE(job.Run()) << "Job was unable to run";

  EXPECT_TRUE(testFunc4GotData) << "Function recieved wrong data";
  EXPECT_TRUE(job.is.Finished()) << "Job is not marked as finished";
}

TEST(JobTests, JobTypeChecks)
{
  JobHandle job1 = Job::Create(TestJob1);
  JobHandle job2 = Job::Create(TestJob2);
  EXPECT_FALSE(job1.is.Type(JobType::Huge)) << "Tiny job was huge";
  EXPECT_FALSE(job1.is.Type(JobType::Misc)) << "Tiny job was misc";
  EXPECT_TRUE(job1.is.Type(JobType::Tiny)) << "Tiny job was not a tiny job";

  EXPECT_TRUE(job2.is.Type(JobType::Huge)) << "Huge job was not huge";
  EXPECT_FALSE(job2.is.Type(JobType::Misc)) << "Huge job was misc";
  EXPECT_FALSE(job2.is.Type(JobType::Tiny)) << "Huge job was tiny";
}
