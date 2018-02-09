#include <gtest/gtest.h>

#include "../source/Job.h"

using namespace JobBot;

bool testFunc1HasRun;
void TestJobFunc1(Job* job) { testFunc1HasRun = true; }
TinyJobFunction TestJob1(TestJobFunc1);

bool testFunc2HasRun;
void TestJobFunc2(Job* job) { testFunc2HasRun = true; }
HugeJobFunction TestJob2(TestJobFunc2);

bool testFunc3GotData;
void TestJobFunc3(Job* job) { testFunc3GotData = (job->GetData<int>() == 4); }
IOJobFunction TestJob3(TestJobFunc3);

bool testFunc4GotData;
void TestJobFunc4(Job* job) { testFunc4GotData = (job->GetData<float>() == 25.12f); }
GraphicsJobFunction TestJob4(TestJobFunc4);

TEST(JobTests, SizeVerification) { ASSERT_EQ((size_t)Job::TARGET_JOB_SIZE, sizeof(Job)) << "Job was incorrect size"; }

TEST(JobTests, Create)
{
  Job* job = Job::Create(TestJob1);

  EXPECT_FALSE(job->IsFinished()) << "Job has not been created correctly";
  EXPECT_FALSE(job->InProgress()) << "Job has not been created correctly";
}

TEST(JobTests, RunJob)
{
  testFunc1HasRun = false;
  Job* job        = Job::Create(TestJob1);

  EXPECT_FALSE(testFunc1HasRun) << "Job has been prematurely executed";
  EXPECT_FALSE(job->IsFinished()) << "Job is marked as finished before it has run";

  job->Run();

  EXPECT_TRUE(testFunc1HasRun) << L"Job has been run but has not executed job code";
  EXPECT_TRUE(job->IsFinished()) << L"Job has been run but is not marked as finished";
  EXPECT_FALSE(job->InProgress()) << L"Job is finished but still marked as in progress";
}

TEST(JobTests, Parent)
{
  testFunc1HasRun = false;
  testFunc2HasRun = false;

  // Create 2 jobs with job2 as a child of job1
  Job* job1 = Job::Create(TestJob1);
  Job* job2 = Job::CreateChild(TestJob2, job1);

  // Make sure that nothing has prematurely fired
  EXPECT_FALSE(testFunc1HasRun) << "Job1 has been prematurely executed";
  EXPECT_FALSE(job1->IsFinished()) << "Job1 is marked as finished before it has run";
  EXPECT_FALSE(testFunc2HasRun) << "Job2 has been prematurely executed";
  EXPECT_FALSE(job2->IsFinished()) << "Job2 is marked as finished before it has run";

  // Run job1 (the parent)
  job1->Run();

  // Job 1 has now run, but should not be marked done because it has a child that has not finished
  EXPECT_TRUE(testFunc1HasRun) << "Job1 has not run correctly";
  EXPECT_FALSE(job1->IsFinished()) << "Job1 is marked as finished before all of its children have finished";
  EXPECT_FALSE(testFunc2HasRun) << "Job2 has been prematurely executed";
  EXPECT_FALSE(job2->IsFinished()) << "Job2 is marked as finished before it has run";

  // Run job2 (the child)
  job2->Run();

  // Make sure all jobs are now correctly marked as completed
  EXPECT_TRUE(testFunc1HasRun) << "Job1 has not run correctly";
  EXPECT_TRUE(job1->IsFinished()) << "Job1 is not marked as finished even though all child jobs are done";
  EXPECT_TRUE(testFunc2HasRun) << "Job2 has not run correctly";
  EXPECT_TRUE(job2->IsFinished()) << "Job2 has been run but is not marked as finished";
}

TEST(JobTests, Callback)
{
  testFunc1HasRun = false;
  testFunc2HasRun = false;

  // Create job1 with a callback function
  Job* job = Job::Create(TestJob1);
  job->SetCallback(TestJob2);

  EXPECT_FALSE(testFunc1HasRun) << "Job has been prematurely executed";
  EXPECT_FALSE(job->IsFinished()) << "Job is marked as finished before it has run";
  EXPECT_FALSE(testFunc2HasRun) << "Callback has been prematurely executed";

  job->Run();

  EXPECT_TRUE(testFunc1HasRun) << "Job has been run but has not executed job code";
  EXPECT_TRUE(job->IsFinished()) << "Job has been run but is not marked as finished";
  EXPECT_TRUE(testFunc2HasRun) << "Job has been run but has not executed callback code";
}

TEST(JobTests, Data1)
{
  testFunc3GotData = false;

  Job* job = Job::Create(TestJob3, 4);

  EXPECT_FALSE(testFunc3GotData) << "Function somehow already got the data even though it hasn't run yet";
  EXPECT_FALSE(job->IsFinished()) << "Job is marked as finished before it has run";

  job->Run();

  EXPECT_TRUE(testFunc3GotData) << "Function did not correctly recieve the data";
  EXPECT_TRUE(job->IsFinished()) << "Job is not marked as finished";
}

TEST(JobTests, Data2)
{
  testFunc4GotData = false;

  Job* job = Job::Create(TestJob4, 25.12f);

  EXPECT_FALSE(testFunc4GotData) << "Function somehow already got the data even though it hasn't run yet";
  EXPECT_FALSE(job->IsFinished()) << "Job is marked as finished before it has run";

  job->Run();

  EXPECT_TRUE(testFunc4GotData) << "Function recieved wrong data";
  EXPECT_TRUE(job->IsFinished()) << "Job is not marked as finished";
}

TEST(JobTests, JobTypeChecks)
{
	Job* job1 = Job::Create(TestJob1);
	Job* job2 = Job::Create(TestJob2);
  EXPECT_FALSE(job1->MatchesType(JobType::Huge)) << "Tiny job was huge";
  EXPECT_FALSE(job1->MatchesType(JobType::Misc)) << "Tiny job was misc";
  EXPECT_TRUE(job1->MatchesType(JobType::Tiny)) << "Tiny job was not a tiny job";

  EXPECT_TRUE(job2->MatchesType(JobType::Huge)) << "Huge job was not huge";
  EXPECT_FALSE(job2->MatchesType(JobType::Misc)) << "Huge job was misc";
  EXPECT_FALSE(job2->MatchesType(JobType::Tiny)) << "Huge job was tiny";
}
