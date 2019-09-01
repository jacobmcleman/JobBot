/**************************************************************************
    Exceptions for job system to throw when things do not go as planned

    Author:    Jake McLeman
***************************************************************************/
#include <string>

#include "Job.h"
#include "JobExceptions.h"

namespace JobBot
{
JobRejected::JobRejected(FailureType mode, JobHandle job)
    : mode_(mode), guiltyJob_(job)
{
  if (guiltyJob_ != nullptr)
  {
    guiltyJob_->SetAllowCompletion(false);
  }
}

JobRejected::~JobRejected()
{
  if (guiltyJob_ != nullptr)
  {
    guiltyJob_->SetAllowCompletion(true);
  }
}

const char* JobRejected::what() const throw()
{
  std::string error;
  error += "Job was rejected by worker, ";
  switch (mode_)
  {
  case FailureType::NullJob:
    error += "given job was null";
    break;
  case FailureType::QueueFull:
    error += "worker's queue was full";
    break;
  default:
    error += "reason was unknown";
    break;
  }
  return nullptr;
}

JobRejected::FailureType JobRejected::GetFailureMode() const { return mode_; }

JobHandle JobRejected::GetJob() const { return guiltyJob_; }
}
