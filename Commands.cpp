#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <fcntl.h>
#include <algorithm>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}



// TODO: Add your implementation for classes in Commands.h 
Command::Command(const char *cmd_line):cmd_line(cmd_line)
{
  argv = new char*[COMMAND_MAX_ARGS+1];
  argc = _parseCommandLine(cmd_line, argv);


}



void ChangePromptCommand::execute()
{
  SmallShell& shell = SmallShell::getInstance();
  std::string newP = "smash";
  if(argc <= 1)
  {
      shell.setPrompt(newP);
  }
  else
  {
      newP = argv[1];
      shell.setPrompt(newP);
  }
}

void ShowPidCommand::execute()
{
    SmallShell& shell = SmallShell::getInstance();
    std::cout << "smash pid is " << shell.getSmashPID() << std::endl;
}

void GetCurrDirCommand::execute()
{
    char* buff = new char[FILENAME_MAX];

    if(!getcwd(buff, FILENAME_MAX)) //Get the current directory path
    {
        perror("smash error: getcwd failed");
    }

    std::cout << buff << std::endl;
    free(buff);
}

void ChangeDirCommand::execute()
{
    if(argc > 2)
    {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }

    char* buf = new char[FILENAME_MAX];
    char* lastDir = *(SmallShell::getInstance().last_dir_path);

    if(!strcmp(argv[1], "-"))
    {
        if(lastDir == nullptr)
        {
            std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            return;
        }

        //now we set the current dir to be the last one
        if(!getcwd(buf, FILENAME_MAX))
        {
            perror("smash error: getcwd failed");
            return;
        }

        if(chdir(lastDir) < 0)
        {
            perror("smash error: chdir failed");
            return;
        }
    }
    else
    {
        if(!getcwd(buf, FILENAME_MAX))
        {
            perror("smash error: chdir failed");
            return;
        }

        if(chdir(argv[1]) < 0)
        {
            perror("smash error: chdir failed");
            return;
        }
    }

    delete[] SmallShell::getInstance().last_dir_path;
    SmallShell::getInstance().last_dir_path = new char *[FILENAME_MAX];
    *(SmallShell::getInstance().last_dir_path) = buf;
}

void ForegroundCommand::execute()
{
    foregroundJobs->removeFinishedJobs();
    SmallShell& shell = SmallShell::getInstance();
    int jobId;

    if(argc > 2)
    {
        std::cerr << "smash error: fg: invalid arguments" << std::endl;
        return;
    }

   if(argc == 1) //No PID supported
   {
       jobId = this->foregroundJobs->getMaxJobID();
   }
   else if(argc == 2)
   {
       jobId = stoi(argv[1]);

       if(jobId == 0)
       {
           std::cerr << "smash error: fg: job-id " << 0 << " does not exist" << std::endl;
           return;
       }

       if(!jobId)
       {
           std::cerr << "smash error: fg: invalid arguments" << std::endl;
           return;
       }
   }

   if(foregroundJobs->getJobs().empty())
   {
       std::cerr << "smash error: fg: jobs list is empty" << std::endl;
       return;
   }

   JobsList::JobEntry* job = foregroundJobs->getJobById(jobId);

   if(job == nullptr)
   {
       std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
       return;
   }

   if(job->is_stopped)
   {
       int killSYSCALL_res = kill(job->pid, SIGCONT);

       if(killSYSCALL_res == -1)//the syscall failed
       {
           std::perror("smash error: kill failed");
           return;
       }
   }

   std::cout << job->command->cmd_line << " : " << job->pid << std::endl;
   shell.setCurrentJobPID(job->pid);
   shell.setCurrentCommandLine(std::string(job->command->cmd_line));

   int status;

   if(waitpid(job->pid, &status, WUNTRACED) == -1)
   {
       std:perror("smash error: waitpid failed");
       return;
   }
}

void BackgroundCommand::execute()
{
    this->backgroundJobs->removeFinishedJobs();
    SmallShell& shell = SmallShell::getInstance();
    int jobId;

    if(argc > 2)
    {
        std::cerr << "smash error: bg: invalid arguments" << std::endl;
        return;
    }

    JobsList::JobEntry* job = nullptr;

    if(argc == 2)
    {
        jobId = stoi(argv[1]);

        if(jobId == 0)
        {
            std::cerr << "smash error: bg: job-id " << 0 << " does not exist" << std::endl;
            return;
        }

        if(!jobId)
        {
            std::cerr << "smash error: bg: invalid arguments" << std::endl;
            return;
        }

        job = backgroundJobs->getJobById(jobId);
        
    }
}
void JobsCommand::execute()
{
    jobs->removeFinishedJobs();
    jobs->printJobsList();
}

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped)
{
    removeFinishedJobs();
    JobEntry* job = new JobEntry(getMaxJobID() + 1, pid, isStopped, cmd);
    jobs.push_back(job);
}

static bool jobsSort(JobsList::JobEntry* job1, JobsList::JobEntry* job2) { return (*job1 < *job2); }

void JobsList::printJobsList() {
    std::sort(jobs.begin(), jobs.end(), jobsSort); //Sort the jobs by id

    for (JobEntry* job:jobs) {
        if (job->is_stopped) {
            std::cout << "[" << job->job_id << "] " << job->command->cmd_line << " : " << job->pid << " " <<
                      (int) difftime(time(nullptr), job->elapsed_time) << " secs (stopped)" << std::endl;
        }
        else {
            std::cout << "[" << job->job_id << "] " << job->command->cmd_line << " : " << job->pid << " " <<
                      (int) difftime(time(nullptr), job->elapsed_time) << " secs" << std::endl;
        }
    }
}

void JobsList::removeFinishedJobs()
{
    int index = 0;

    for (JobEntry* job:jobs) {
        int waitResult = waitpid(job->pid, nullptr, WNOHANG);
        if (waitResult != 0) {
            jobs.erase(jobs.begin() + index);
        }
        index++;
    }
}

void JobsList::finishedJobs() {
    SmallShell& shell = SmallShell::getInstance();
    pid_t pid_smash = getpid();

    if(pid_smash < 0) {
        std::perror("smash error: getpid failed");
        return;
    }

    if(pid_smash != shell.getSmashPID()){
        return;
    }

    for(unsigned int i = 0; i < jobs.size(); i++)
    {
        JobsList::JobEntry* job = jobs[i];
        int waitStatus = waitpid(job->pid, nullptr, WNOHANG);
        if(waitStatus != 0) {
            jobs.erase(jobs.begin() + i);
        }
    }

    if(jobs.empty()) {
        jobCounter = 0;
    }
    else {
        jobCounter = jobs.back()->pid;
    }
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for (JobEntry* job:jobs) {
        if (job->job_id == jobId) {
            return job;
        }
    }

    return nullptr;
}

JobsList::JobEntry *JobsList::getJobByPID(int jobPID) {
    for (JobEntry* job:jobs) {
        if (job->pid == jobPID) {
            return job;
        }
    }

    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    for (auto it = jobs.begin(); it != jobs.end(); it++) {
        if ((*it)->job_id == jobId) {
            jobs.erase(it);
            return;
        }
    }
}

void JobsList::killAllJobs()
{
    removeFinishedJobs();

    for (JobEntry* job:jobs) {
        if (kill(job->pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }
    }

    if (!jobs.empty())
    {
        jobs.clear();
    }

    removeFinishedJobs();
}

int JobsList::getMaxJobID() {
    int maxID = 0;
    for (JobEntry* job:jobs) {
        if (job->job_id > maxID) {
            maxID = job->job_id;
        }
    }

    return maxID;
}

JobsList::JobEntry* JobsList::getLastJob(int *lastJobId) {
    if (jobs.empty() || !lastJobId) {
        return nullptr;
    }
    *lastJobId = getMaxJobID();
    return getJobById(*lastJobId);
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    int index = 0, maxID = 0;
    JobEntry* lastStoppedJob = nullptr;

    for (JobEntry* job:jobs) {
        if ((job->job_id > maxID) && job->is_stopped) {
            maxID = job->job_id;
            lastStoppedJob = job;
        }
    }

    *jobId = maxID;

    return lastStoppedJob;
}

SmallShell::SmallShell() {
    this->prompt = "smash";
    this->smashPID = getpid();
    this->currentJobPID = -1;
    this->last_dir_path = new char *[FILENAME_MAX];
    *this->last_dir_path = nullptr;

}

SmallShell::~SmallShell()
{
    delete[] last_dir_path;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

Command * SmallShell::CreateCommand(const char* cmd_line) {
    if(!cmd_line)
    {
        return nullptr;
    }
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord.compare("chprompt") == 0)
    {
        return new ChangePromptCommand(cmd_line);
    }
    else if(firstWord == "showpid")
    {
        return new ShowPidCommand(cmd_line);
    }
    else if(firstWord == "pwd")
    {
        return new GetCurrDirCommand(cmd_line);
    }
    else if(firstWord == "cd")
    {
        return new ChangeDirCommand(cmd_line, last_dir_path);
    }
    else if(firstWord == "jobs")
    {
        return new JobsCommand(cmd_line, &jobs);
    }
    else if(firstWord == "fg")
    {
        return new ForegroundCommand(cmd_line, &this->jobs);
    }
	// For example:
/*
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
    Command* cmd = CreateCommand(cmd_line);

    if(cmd_line)
    {
        cmd->execute();
        delete cmd;
    }
}


