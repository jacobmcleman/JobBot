/**************************************************************************
    A few general jobs for use with JobBot

    Author: Jake McLeman


    All content copyright 2017 DigiPen (USA) Corporation, all rights reserved.
***************************************************************************/
#ifndef _JOBUTILITY_H
#define _JOBUTILITY_H

#include <algorithm>

#include "Job.h"
#include "Manager.h"

namespace JobBot
{
class Utilities
{
public:
  /*
      Specialized type of job for use with parallel for that takes some extra
     data

      dataChunk - array of the data that this job should execute over
      chunkSize - number of elements in that array

      Note that the nature of this function means there are likely elements
     before and after
      the assigned data chunk, and accessing them will likely not cause an
     error, but it is
      NOT THREAD SAFE
  */
  template <typename T>
  using ParallelForJobFunction = void (*)(Job* job, T* dataChunk,
                                          size_t chunkSize);

  /*
      Creates a parallel for job

      Parallel-For executes the given ParallelForJobFunction on each element in
     the data array
      A job is created to handle every *chunkSize* elements
  */
  template <typename T>
  static Job* ParallelForJob(Manager* manager,
                             ParallelForJobFunction<T> function, T* data,
                             size_t size, size_t chunkSize);

private:
  /*
      Data for the job that does the splitting into jobs
  */
  template <typename T> struct ParallelForSplitterData
  {
    ParallelForJobFunction<T> function;
    T* data;
    size_t size;
    size_t chunkSize;
    Manager* manager;
  };

  /*
      Data for the parallel for job itself
  */
  template <typename T> struct ParallelForJobData
  {
    ParallelForJobFunction<T> function;
    T* data;
    size_t size;
  };

  /*
      Job function to handle the actual splitting off of all
      the jobs
  */
  template <typename T> static void ParallelForSplitterFunction(Job* job);

  /*
      Job function that hands of the call to the tidier interface
      of the users ParallelForJobFunction
  */
  template <typename T> static void ParallelForJob(Job* job);
};

template <typename T>
inline Job* Utilities::ParallelForJob(Manager* manager,
                                      ParallelForJobFunction<T> function,
                                      T* data, size_t size, size_t chunkSize)
{
  ParallelForSplitterData<T> splitterData = {function, data, size, chunkSize,
                                             manager};
  return Job::Create<ParallelForSplitterData<T>>(ParallelForSplitterFunction<T>,
                                                 splitterData);
}

template <typename T>
inline void Utilities::ParallelForSplitterFunction(Job* job)
{
  ParallelForSplitterData<T>& jobData =
      job->GetData<ParallelForSplitterData<T>>();

  job->SetAllowCompletion(false);

  for (size_t i = 0; i < jobData.size; i += jobData.chunkSize)
  {
    // Create data for a parallel for job
    ParallelForJobData<T> data = {
        jobData.function, jobData.data + i,
        std::min(jobData.chunkSize, jobData.size - i)};
    // Send job with data to the manager
    jobData.manager->SubmitJob(
        Job::CreateChild<ParallelForJobData<T>>(ParallelForJob<T>, data, job));
  }

  job->SetAllowCompletion(true);
}

template <typename T> inline void Utilities::ParallelForJob(Job* job)
{
  ParallelForJobData<T>& data = job->GetData<ParallelForJobData<T>>();

  // Run the parallel for job with all needed arguments
  data.function(job, data.data, data.size);
}
}
