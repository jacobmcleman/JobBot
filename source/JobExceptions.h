/**************************************************************************
Exceptions for job system to throw when things do not go as planned

Author:    Jake McLeman


All content copyright 2017 DigiPen (USA) Corporation, all rights reserved.
***************************************************************************/
#ifndef _JOBEXCEPTIONS_H
#define _JOBEXCEPTIONS_H

#include <exception>

namespace JobBot {
// Forward declaration of Job class
class Job;

class JobRejected : public std::exception {
public:
  // Descriptor for why the job was rejected
  enum struct FailureType { QueueFull, NullJob, Unknown };

  /*
      Constructor
      mode - Why the job was rejected
      job - The job that was rejected
  */
  JobRejected(FailureType mode, Job *job);

  /*
      Destructor
  */
  ~JobRejected();

  /*
      Gives back a string to say what went wrong in case
      anyone is interested
  */
  virtual const char *what() const throw();

  /*
      General type of failure
  */
  FailureType GetFailureMode() const;

  /*
      Get the job that caused this whole mess
  */
  Job *GetJob() const;

private:
  FailureType mode_;
  Job *guiltyJob_;
};
}
#endif
