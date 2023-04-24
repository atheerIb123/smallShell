#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
// TODO: Add your data members
public:
    const char* cmd_line;
    char** argv;
    int argc;
    Command(const char* cmd_line) : cmd_line(cmd_line){}
    virtual ~Command(){delete[] argv;}
    virtual void execute() = 0;
    //virtual void prepare();
    // virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line):Command(cmd_line){}
    virtual ~BuiltInCommand() {}
};

class JobsList;
class ExternalCommand : public Command {
private:
    JobsList* jobs;
    bool isBg;
public:
    ExternalCommand(const char* cmd_line, JobsList* jobs, bool isBg):Command(cmd_line), jobs(jobs), isBg(isBg){}
    virtual ~ExternalCommand() {}
    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char* cmd_line);
    virtual ~PipeCommand() {}
    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char* cmd_line);
    virtual ~RedirectionCommand() {}
    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};


class ChangePromptCommand : public BuiltInCommand {
public:
// TODO: Add your data members public:
    ChangePromptCommand(const char* cmd_line): BuiltInCommand(cmd_line){}
    virtual ~ChangePromptCommand(){}
    void execute() override;

};


class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members
private:
    char* currentDir;
    char** lastDirPath;
public:
    ChangeDirCommand(const char* cmd_line, char** plastPwd): BuiltInCommand(cmd_line), lastDirPath(plastPwd)
    {
        this->currentDir = new char[FILENAME_MAX];
    }

    virtual ~ChangeDirCommand()
    {
        delete[] this->currentDir;
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line): BuiltInCommand(cmd_line){}
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
private:
    JobsList* jobs;
public:
    QuitCommand(const char* cmd_line, JobsList* jobs) :BuiltInCommand(cmd_line),jobs(jobs){}
    virtual ~QuitCommand() {}
    void execute() override;
};


class JobsList {
public:
    class JobEntry{
    public:
        int job_id;
        pid_t pid;
        time_t elapsed_time;
        bool is_stopped;
        Command* command;

        JobEntry(int jobID, pid_t pid, bool isStopped, Command* cmd) : job_id(jobID), pid(pid), elapsed_time(time(nullptr)), is_stopped(isStopped), command(cmd) {}
        JobEntry& operator=(const JobEntry& job_entry) = default;
        JobEntry(const JobEntry& other) = default;
        ~JobEntry() = default;

        bool operator<(const JobsList::JobEntry& job) const { return (this->job_id < job.job_id); }
        bool operator>(const JobsList::JobEntry& job) const { return (this->job_id > job.job_id); }
        bool operator==(const JobsList::JobEntry& job) const { return (this->job_id == job.job_id); }
    };

    JobsList() = default;
    JobsList(const JobsList& other) = default;
    JobsList& operator=(const JobsList& job_list) = default;
    ~JobsList() = default;

    void addJob(Command* cmd, pid_t pid, bool isStopped);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    void finishedJobs();
    JobEntry* getJobById(int jobId);
    JobEntry* getJobByPID(int jobPID);
    void removeJobById(int jobId);
    int getMaxJobID();
    JobEntry* getLastJob(int* lastJobId);
    JobEntry* getLastStoppedJob(int *jobId);
    std::vector<JobEntry*> getJobs() { return jobs; }
private:
    std::vector<JobEntry*> jobs;
    int jobCounter = 0;
};

class JobsCommand : public BuiltInCommand {
private:
    JobsList* jobs;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    virtual ~JobsCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
private:
    JobsList* foregroundJobs;
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), foregroundJobs(jobs){}
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
private:
    JobsList* backgroundJobs;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), backgroundJobs(jobs) {}
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class TimeoutCommand : public BuiltInCommand {
/* Bonus */
// TODO: Add your data members
public:
    explicit TimeoutCommand(const char* cmd_line);
    virtual ~TimeoutCommand() {}
    void execute() override;
};

class ChmodCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ChmodCommand(const char* cmd_line);
    virtual ~ChmodCommand() {}
    void execute() override;
};

class GetFileTypeCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    GetFileTypeCommand(const char* cmd_line);
    virtual ~GetFileTypeCommand() {}
    void execute() override;
};

class SetcoreCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    SetcoreCommand(const char* cmd_line);
    virtual ~SetcoreCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
private:
    JobsList* jobs;
public:
    KillCommand(const char* cmd_line, JobsList* jobs):BuiltInCommand(cmd_line), jobs(jobs){}
    virtual ~KillCommand() {}
    void execute() override;
    bool validArguments();
};

class SmallShell {
private:
    pid_t smashPID; //for showPID
    pid_t currentJobPID;

    std::string prompt; //for chprompt
    std::string currentCommandLine;
    SmallShell();

public:
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    void setPrompt(std::string& newPrompt)
    {
        this->prompt = newPrompt;
    }
    std::string& getPrompt()
    {
        return this->prompt;
    }
    pid_t getSmashPID()
    {
        return this->smashPID;
    }
    pid_t getCurrentJobPID()
    {
        return this->currentJobPID;
    }
    void setCurrentJobPID(pid_t newID)
    {
        this->currentJobPID = newID;
    }

    void setCurrentCommandLine(std::string cmd_line)
    {
        this->currentCommandLine = cmd_line;
    }

    void executeCommand(const char* cmd_line);
    char** last_dir_path; //for CD
    JobsList jobs;
};

#endif //SMASH_COMMAND_H_
