#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
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

JobsList::JobEntry& JobsList::JobEntry::operator=(const JobsList::JobEntry& job_entry) {
    if (this == &job_entry) {
        return *this;
    }

    job_id = job_entry.job_id;
    pid = job_entry.pid;
    elapsed_time = job_entry.elapsed_time;
    is_stopped = job_entry.is_stopped;
    command = job_entry.command;

    return *this;
}

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
    for (std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
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
Command::Command(const char* cmd_line) :cmdLine(std::string(cmd_line))
{
    argv = new char* [COMMAND_MAX_ARGS + 1];
    argc = _parseCommandLine(cmd_line, argv);


}



void ChangePromptCommand::execute()
{
    SmallShell& shell = SmallShell::getInstance();
    std::string newP = "smash";
    if (argc <= 1)
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

    if (!getcwd(buff, FILENAME_MAX)) //Get the current directory path
    {
        perror("smash error: getcwd failed");
    }

    std::cout << buff << std::endl;
    free(buff);
}

void ChangeDirCommand::execute()
{
    if (argc > 2)
    {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }

    char* buf = new char[FILENAME_MAX];
    char* lastDir = *(SmallShell::getInstance().last_dir_path);

    if (!strcmp(argv[1], "-"))
    {
        if (lastDir == nullptr)
        {
            std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            return;
        }

        //now we set the current dir to be the last one
        if (!getcwd(buf, FILENAME_MAX))
        {
            perror("smash error: getcwd failed");
            return;
        }

        if (chdir(lastDir) < 0)
        {
            perror("smash error: chdir failed");
            return;
        }
    }
    else
    {
        if (!getcwd(buf, FILENAME_MAX))
        {
            perror("smash error: chdir failed");
            return;
        }

        if (chdir(argv[1]) < 0)
        {
            perror("smash error: chdir failed");
            return;
        }
    }

    delete[] SmallShell::getInstance().last_dir_path;
    SmallShell::getInstance().last_dir_path = new char* [FILENAME_MAX];
    *(SmallShell::getInstance().last_dir_path) = buf;
}

void ForegroundCommand::execute()
{
    foregroundJobs->removeFinishedJobs();
    SmallShell& shell = SmallShell::getInstance();
    int jobId;

    if (argc > 2)
    {
        std::cerr << "smash error: fg: invalid arguments" << std::endl;
        return;
    }

    if (argc == 1) //No PID supported
    {
        jobId = this->foregroundJobs->getMaxJobID();
    }
    else if (argc == 2)
    {
        for (char& c : std::string(argv[1]))
        {
            if (c == '-')
            {
                continue;
            }
            if(!std::isdigit(c))
            {
                std::cerr << "smash error: fg: invalid arguments" << std::endl;
                return;
            }
        }

        jobId = stoi(argv[1]);

        if (jobId == 0)
        {
            std::cerr << "smash error: fg: job-id " << 0 << " does not exist" << std::endl;
            return;
        }

        if (!jobId)
        {
            std::cerr << "smash error: fg: invalid arguments" << std::endl;
            return;
        }
    }

    if (foregroundJobs->getJobs().empty() && !this->argv[1])
    {
        std::cerr << "smash error: fg: jobs list is empty" << std::endl;
        return;
    }

    JobsList::JobEntry* job = foregroundJobs->getJobById(jobId);

    if (job == nullptr)
    {
        std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
        return;
    }



    if (job->is_stopped)
    {
        int killSYSCALL_res = kill(job->pid, SIGCONT);

        if (killSYSCALL_res == -1)//the syscall failed
        {
            std::perror("smash error: kill failed");
            return;
        }
    }

    std::cout << job->command->cmdLine << " : " << job->pid << std::endl;
    shell.setCurrentJobPID(job->pid);
    shell.setCurrentCommandLine(std::string(job->command->cmdLine));

    int status;

    if (waitpid(job->pid, &status, WUNTRACED) == -1)
    {
        std::perror("smash error: waitpid failed");
        return;
    }
}

void BackgroundCommand::execute()
{
    this->backgroundJobs->removeFinishedJobs();
    int jobId;

    if (argc > 2)
    {
        std::cerr << "smash error: bg: invalid arguments" << std::endl;
        return;
    }

    JobsList::JobEntry* job = nullptr;

    if (argc == 2)
    {
        jobId = stoi(argv[1]);

        if (jobId == 0)
        {
            std::cerr << "smash error: bg: job-id " << 0 << " does not exist" << std::endl;
            return;
        }

        if (!jobId)
        {
            std::cerr << "smash error: bg: invalid arguments" << std::endl;
            return;
        }

        job = backgroundJobs->getJobById(jobId);
        if (job == nullptr)
        {
            std::cerr << "smash error: bg: job-id " << jobId << " does not exist" << std::endl;
            return;
        }
        if (!job->is_stopped)
        {
            std::cerr << "smash error: bg: job-id " << jobId << " is already running in the background" << std::endl;
            return;
        }
    }
    else if (argc == 1)
    {
        job = backgroundJobs->getLastStoppedJob(&jobId);
        if (!job)
        {
            std::cerr << "smash error: bg: there is no stopped jobs to resume" << std::endl;
            return;
        }
    }
    job->is_stopped = false;
    std::cout << job->command->cmdLine << " : " << job->pid << std::endl;
    int killSYSCALL_res = kill(job->pid, SIGCONT);

    if (killSYSCALL_res == -1)//the syscall failed
    {
        std::perror("smash error: kill failed");
        return;
    }
}

void QuitCommand::execute()
{
    if (argv[1] && strcmp(argv[1], "kill") == 0)
    {
        jobs->removeFinishedJobs();
        std::cout << "smash: sending SIGKILL signal to " << jobs->getJobs().size() << " jobs:" << std::endl;

        for (JobsList::JobEntry* job : jobs->getJobs())
        {
            std::cout << job->pid << ": " << job->command->cmdLine << std::endl;
        }

        jobs->killAllJobs();
    }

    exit(0);
}
void KillCommand::execute()
{
    jobs->removeFinishedJobs();

    if (!validArguments())
    {
        return;
    }

    int id = stoi(argv[2]);
    int sigNum = stoi(argv[1]);
    sigNum = -sigNum;//check

    JobsList::JobEntry* job = jobs->getJobById(id);

    if (!job)
    {
        cerr << "smash error: kill: job-id " << id << " does not exist" << endl;
        return;
    }

    if (kill(job->pid, sigNum) == -1)
    {
        cerr << "smash error: kill: invalid arguments" << endl;
    }
    else
    {
        std::cout << "signal number " << sigNum << " was sent to pid " << job->pid << std::endl;
    }

}

bool KillCommand::validArguments()
{
    //check
    if (argv[1] == nullptr || argv[2] == nullptr || argv[1][0] != '-' ||
        argv[1][1] < '0' || argv[1][1] > '9' || argv[3] != nullptr)
    {
        cerr << "smash error: kill: invalid arguments" << endl;
        return false;
    }
    int i = 2;
    while (argv[1][i] != '\0') //make sure the sig is a number
    {
        if (argv[1][i] < '0' || argv[1][i] > '9')
        {
            cerr << "smash error: kill: invalid arguments" << endl;
            return false;
        }
        i++;
    }
    i = 0;
    if (argv[2][0] == '\0')
    {
        cerr << "smash error: kill: invalid arguments" << endl;
        return false;
    }

    if (argv[2][0] == '-')
    {
        i = 1;
    }

    while (argv[2][i] != '\0') //make sure the pid is a number
    {

        if (argv[2][i] < '0' || argv[2][i] > '9')
        {
            cerr << "smash error: kill: invalid arguments" << endl;
            return false;
        }

        i++;

    }

    i = 1;
    int num = 0;
    if (argv[2] != nullptr)
    {
        while (argv[2][i] != '\0')
        {
            num = 10 * num + argv[1][i++] - '0';
        }
    }


    if (argv[2][0] == '-')
    {
        cerr << "smash error: kill: job-id " << stoi(argv[2]) << " does not exist" << endl;
        return false;
    }

    return true;

}


static bool isComplex(std::string cmd_line)
{
    return ((cmd_line.find('*') != std::string::npos) || (cmd_line.find('?') != std::string::npos));
}

//special execution :)
void ExternalCommand::execute()
{
    SmallShell& shell = SmallShell::getInstance();
    pid_t pid = fork();

    if (pid == -1)
    {
        std::perror("smash error: fork failed");
        return;
    }

    if (pid == 0) //child
    {
        if (setpgrp() == -1)
        {
            std::perror("smash error: setpgrp failed");
            return;
        }

        char destination[COMMAND_ARGS_MAX_LENGTH + 1];

        strcpy(destination, cmdLine.c_str());

        if (isComplex(std::string(cmdLine)))
        {
            char* arg[] = { (char*)"/bin/bash", (char*)"-c", destination, nullptr };

            if (execv("/bin/bash", arg) == -1)
            {
                std::perror("smash error: execv failed");
                return;
            }

        }
        else
        {

            if (isBg)
            {
                _removeBackgroundSign(destination);
            }

            char* args[COMMAND_ARGS_MAX_LENGTH + 1] = { nullptr };
            _parseCommandLine(destination, args);

            if (execvp(args[0], args) == -1)
            {
                std::perror("smash error: execvp failed");
                return;
            }

        }
    }
    else //father
    {

        if (!isBg)
        {
            int status = 0;

            shell.setCurrentJobPID(pid);
            shell.setCurrentCommandLine(std::string(cmdLine));

            int waitResult = waitpid(pid, &status, WUNTRACED);

            if (waitResult == -1)
            {
                perror("smash error: waitpid failed");
                return;
            }

        }
        else
        {
            ExternalCommand* exCmd = new ExternalCommand(this->cmdLine.c_str(), jobs, isBg);
            shell.setCurrentJobPID(-1);
            jobs->addJob(exCmd, pid, false);
        }

    }
}


void TimedJobs::removeKilledJobs()
{
    SmallShell& shell = SmallShell::getInstance();
    pid_t pid_smash = getpid();

    if (pid_smash != shell.getSmashPID())
    {
        return;
    }

    for (unsigned int i = 0; i < shell.timed_jobs->timeouts.size(); i++)
    {
        TimedEntry* timedEntry = shell.timed_jobs->timeouts[i];
        time_t curr_time = time(nullptr);

        if (timedEntry->getEndTime() <= curr_time)
        {
            delete shell.timed_jobs->timeouts[i];
            shell.timed_jobs->timeouts.erase(shell.timed_jobs->timeouts.begin() + i);
        }
    }
    shell.jobs.finishedJobs();
}

void TimedJobs::modifyJobByID(pid_t job_pid)
{
    int timed_jobs_number = timeouts.size();
    if (timed_jobs_number >= 1) {

        timeouts[timed_jobs_number - 1]->setPid(job_pid);
    }
    else
    {
        timeouts[timed_jobs_number]->setPid(job_pid);
    }

    std::sort(timeouts.begin(), timeouts.end(), timeoutEntryIsBigger);
}

void TimeoutCommand::getDataFromTimeOutCommand(const char* cmd)
{
    if (!cmd) {
        return;
    }

    if (argc < 3)
    {
        error_type = TOO_FEW_ARGS;
        return;
    }

    std::string timeoutDuration = string(argv[1]);

    for (unsigned int index = 0; index < timeoutDuration.size(); index++)
    {
        if (timeoutDuration[index] < '0' || timeoutDuration[index] > '9')
        {
            error_type = TIMEOUT_NUMBER_IS_NOT_INTEGER;
            return;
        }
    }

    timer = atoi(argv[1]);
    string cmd_temp;

    for (int i = 2; i < argc; i++)
    {
        cmd_temp += argv[i];
        cmd_temp += " ";
    }

    command = _trim(cmd_temp);
}

void TimeoutCommand::execute()
{
    this->getDataFromTimeOutCommand(this->cmdLine.c_str());

    if (error_type == TIMEOUT_NUMBER_IS_NOT_INTEGER || error_type == TOO_FEW_ARGS)
    {
        std::cerr << "smash error: timeout: invalid arguments" << std::endl;
        return;
    }

    SmallShell& shell = SmallShell::getInstance();
    TimedEntry* current_timed_out = new TimedEntry(command, -1, time(nullptr), timer, false);

    shell.timed_jobs->timeouts.push_back(current_timed_out);
    shell.setAlarmedJobs(true);

    char fgCommand[COMMAND_ARGS_MAX_LENGTH];
    strcpy(fgCommand, command.c_str());
    _removeBackgroundSign(fgCommand);

    bool is_bg = _isBackgroundComamnd(this->command.c_str());
    pid_t pid = fork();

    if (pid < 0)
    {
        std::perror("smash error: fork failed");
        return;
    }
    else if (pid == 0)
    {
        setpgrp();
        shell.executeCommand(fgCommand);
        while (wait(nullptr) != -1) {}
        exit(0);
    }
    else
    {
        if (shell.getSmashPID() == getpid())
        {
            shell.setAlarmedJobs(true);
            shell.timed_jobs->modifyJobByID(pid);

            time_t current_time = time(nullptr);
            TimedEntry* alarm_entry = shell.timed_jobs->timeouts[0];
            time_t end_time = alarm_entry->getEndTime();

            unsigned int seconds = end_time - current_time;
            alarm(seconds);

            if (is_bg)
            {
                TimeoutCommand* copy_command = new TimeoutCommand(*this);
                shell.jobs.addJob(copy_command, pid, false);
            }
            else
            {
                if (waitpid(pid, nullptr, WUNTRACED) == -1)
                {
                    std::perror("smash error: waitpid failed");
                    return;
                }
            }
            shell.setAlarmedJobs(false);
        }
    }
    shell.setAlarmedJobs(false);
}

void RedirectionCommand::getData(std::string cmd) {

    if (cmd.empty())
    {
        return;
    }

    if (cmd.find(">>") != std::string::npos)
    {
        isAppend = true;
    }
    else
    {
        isAppend = false;
    }

    /*int i = 0;
    while(cmd[i] != '>')
    {
        i++;
    }*/

    int beginPos = cmd.find(">");
    int i = beginPos;
    if (isAppend)
    {
        beginPos += 2;
    }
    else
    {
        beginPos += 1;
    }

    string path_temp = cmd.substr(beginPos, cmd.size() - beginPos + 1);
    string command_temp = cmd.substr(0, i);

    destination = _trim(path_temp);
    this->currentCmd = _trim(command_temp);
}





void RedirectionCommand::prepare()
{
    getData(cmdLine);
    dupStdOut = dup(1);

    if (dupStdOut == -1)
    {
        std::perror("smash error: dup failed");
        return;
    }

    if (close(1) == -1)
    {
        close(dupStdOut);
        std::perror("smash error: close failed");
        return;
    }

    int currentFD = -1;

    if (isAppend)
    {
        currentFD = open(destination.c_str(), O_CREAT | O_RDWR | O_APPEND, 0655);
        if (currentFD == -1)
        {

            if (dup2(dupStdOut, 1) == -1)
            {
                std::perror("smash error: dup2 failed");
                return;
            }

            if (close(dupStdOut) == -1)
            {
                std::perror("smash error: close failed");
                return;
            }

            dupStdOut = -1;
            isFailed = true;
            std::perror("smash error: open failed");
            return;
        }
    }
    else
    {
        currentFD = open(destination.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0655);
        if (currentFD == -1)
        {

            if (dup2(dupStdOut, 1) == -1)
            {
                std::perror("smash error: dup2 failed");
                return;
            }

            if (close(dupStdOut) == -1)
            {
                std::perror("smash error: close failed");
                return;
            }

            dupStdOut = -1;
            isFailed = true;
            std::perror("smash error: open failed");
            return;
        }
    }

    if (dup(currentFD) == -1)
    {
        cleanup();
        isFailed = true;
        std::perror("smash error: dup failed");
        return;
    }

    isFailed = false;
}

void RedirectionCommand::cleanup()
{
    if (close(1) == -1) //try closing 2-3
    {
        std::perror("smash error: close failed");
        return;
    }

    if (dup2(dupStdOut, 1) == -1)
    {
        std::perror("smash error: dup2 failed");
        return;
    }

    if (close(dupStdOut) == -1)
    {
        std::perror("smash error: close failed");
        return;
    }

    dupStdOut = -1;
}

void RedirectionCommand::execute()
{
    prepare();

    if (isFailed)
    {
        return;
    }

    SmallShell& shell = SmallShell::getInstance();
    shell.executeCommand(this->currentCmd.c_str());
    cleanup();

}

void PipeCommand::prepare()
{
    size_t pipe_index = string(cmdLine).find('|');

    if (type == stdErr)
    {
        pipe_index = string(this->cmdLine).find("|&");
    }

    string cmd_splitted1 = string(cmdLine);
    string cmd_splitted2 = string(cmdLine);

    command1 = cmd_splitted1.substr(0, pipe_index);
    pipe_index++;
    if (type == stdErr)
    {
        pipe_index++;
    }

    string str_temp = cmd_splitted2.substr(pipe_index, cmd_splitted1.length() - pipe_index);
    str_temp = _trim(str_temp);
    str_temp += " ";
    command2 = str_temp.substr(0, str_temp.find_last_of(WHITESPACE));
}

void PipeCommand::execute()
{
    prepare();
    SmallShell& shell = SmallShell::getInstance();
    int fd_pipe[2];

    if (pipe(fd_pipe) == -1)
    {
        std::perror("smash error: pipe failed");
        return;
    }

    pid_t firstChild_pid = fork();

    if (firstChild_pid == -1)
    {
        std::perror("smash error: fork failed");
        return;
    }

    if (firstChild_pid == 0)
    {
        if (setpgrp() == -1)
        {
            std::perror("smash error: setpgrp failed");
            return;
        }

        if (type == stdIn)
        {
            if (dup2(fd_pipe[1], 1) == -1) {
                std::perror("smash error: dup2 failed");
                return;
            }

            if (close(fd_pipe[0]) == -1) {
                std::perror("smash error: close failed");
                return;
            }

            if (close(fd_pipe[1]) == -1) {
                std::perror("smash error: close failed");
                return;
            }
        }
        else
        {
            if (dup2(fd_pipe[1], 2) == -1)
            {
                std::perror("smash error: dup2 failed");
                return;
            }

            if (close(fd_pipe[0]) == -1)
            {
                std::perror("smash error: close failed");
                return;
            }

            if (close(fd_pipe[1]) == -1)
            {
                std::perror("smash error: close failed");
                return;
            }
        }

        Command* pipe_command = shell.CreateCommand(this->command1.c_str());
        pipe_command->execute();
        exit(0);
    }

    pid_t secondChild_pid = fork();

    if (secondChild_pid == -1)
    {
        std::perror("smash error: fork failed");
        return;
    }

    if (secondChild_pid == 0)
    {
        if (setpgrp() == -1)
        {
            std::perror("smash error: setpgrp failed");
            return;
        }

        if (dup2(fd_pipe[0], 0) == -1)
        {
            std::perror("smash error: dup2 failed");
            return;
        }

        if (close(fd_pipe[0]) == -1)
        {
            std::perror("smash error: close failed");
            return;
        }

        if (close(fd_pipe[1]) == -1)
        {
            std::perror("smash error: close failed");
            return;
        }

        Command* my_command = shell.CreateCommand(command2.c_str());
        my_command->execute();
        exit(0);
    }

    if (close(fd_pipe[0]) == -1) {
        std::perror("smash error: close failed");
        return;
    }

    if (close(fd_pipe[1]) == -1) {
        std::perror("smash error: close failed");
        return;
    }

    if (waitpid(firstChild_pid, nullptr, WUNTRACED) == -1)
    {
        std::perror("smash error: waitpid failed");
        return;
    }

    if (waitpid(secondChild_pid, nullptr, WUNTRACED) == -1)
    {
        std::perror("smash error: waitpid failed");
        return;
    }
}

void JobsCommand::execute()
{
    jobs->removeFinishedJobs();
    jobs->printJobsList();
}

void SetcoreCommand::execute()
{
    if (argc != 3 || !stoi(argv[1]) || !stoi(argv[2]))
    {
        std::cerr << "smash error: setcore: invalid arguments" << std::endl;
        return;
    }

    SmallShell& shell = SmallShell::getInstance();
    try
    {
        JobsList::JobEntry* job = shell.jobs.getJobById(stoi(argv[1]));

        if (!job)
        {
            std::cerr << "smash error: setcore: job-id " << argv[1] << " does not exist" << std::endl;
            return;
        }

        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(stoi(argv[2]), &mask);

        if (sched_setaffinity(job->job_id, sizeof(mask), &mask) == -1)
        {
            cerr << "smash error: setcore: invalid core number" << "\r\n";
            return;
        }
    }
    catch (exception& e)
    {
        std::cerr << "smash error: setcore: invalid arguments" << "\r\n";
        return;
    }
}

void GetFileTypeCommand::execute()
{
    if (argc != 2)
    {
        std::cerr << "smash error: gettype: invalid aruments" << std::endl;
        return;
    }

    struct stat file_stats;
    const char* filename = argv[1];

    if (stat(filename, &file_stats) == 0)
    {
        if (file_stats.st_mode & S_IFDIR)
        {
            std::cout << argv[1] << "'s type is " << '"' << "directory" << '"' << " and takes up " << file_stats.st_size << " bytes" << std::endl;
        }
        else if (file_stats.st_mode & S_IFREG)
        {
            std::cout << argv[1] << "'s type is " << '"' << "regular file" << '"' << " and takes up " << file_stats.st_size << " bytes" << std::endl;
        }
        else if (file_stats.st_mode & S_IFCHR)
        {
            std::cout << argv[1] << "'s type is " << '"' << "character file" << '"' << " and takes up " << file_stats.st_size << " bytes" << std::endl;
        }
        else if (file_stats.st_mode & S_IFIFO)
        {
            std::cout << argv[1] << "'s type is " << '"' << "FIFO" << '"' << " and takes up " << file_stats.st_size << " bytes" << std::endl;
        }
        else if (file_stats.st_mode & S_IFBLK)
        {
            std::cout << argv[1] << "'s type is " << '"' << "block device" << '"' << " and takes up " << file_stats.st_size << " bytes" << std::endl;
        }
        else if (file_stats.st_mode & S_IFLNK)
        {
            std::cout << argv[1] << "'s type is " << '"' << "symbolic link" << '"' << " and takes up " << file_stats.st_size << " bytes" << std::endl;
        }
        else if (file_stats.st_mode & S_IFSOCK)
        {
            std::cout << argv[1] << "'s type is " << '"' << "socket" << '"' << " and takes up " << file_stats.st_size << " bytes" << std::endl;
        }
    }
    else
    {
        std::cerr << "smash error: gettype: invalid aruments" << std::endl;
        return;
    }
}

void ChmodCommand::execute()
{
    if (argc != 3)
    {
        std::cerr << "smash error: chmod: invalid aruments" << std::endl;
        return;
    }

    int modeInt = stoi(argv[1]);
    mode_t mode = static_cast<mode_t>(modeInt);

    if (chmod(argv[2], mode) != 0)
    {
        std::cerr << "smash error: chmod: invalid aruments" << std::endl;
        return;
    }
}

void JobsList::addJob(Command* cmd, pid_t pid, bool isStopped)
{
    removeFinishedJobs();
    JobEntry* job = new JobEntry(getMaxJobID() + 1, pid, isStopped, cmd);
    jobs.push_back(job);
}

static bool jobsSort(JobsList::JobEntry* job1, JobsList::JobEntry* job2) { return (*job1 < *job2); }

void JobsList::printJobsList() {
    std::sort(jobs.begin(), jobs.end(), jobsSort); //Sort the jobs by id

    for (JobEntry* job : jobs) {
        if (job->is_stopped) {
            std::cout << "[" << job->job_id << "] " << job->command->cmdLine << " : " << job->pid << " " <<
                (int)difftime(time(nullptr), job->elapsed_time) << " secs (stopped)" << std::endl;
        }
        else {
            std::cout << "[" << job->job_id << "] " << job->command->cmdLine << " : " << job->pid << " " <<
                (int)difftime(time(nullptr), job->elapsed_time) << " secs" << std::endl;
        }
    }
}

void JobsList::removeFinishedJobs()
{
    for (JobEntry* job : jobs) {
        int waitResult = waitpid(job->pid, nullptr, WNOHANG);
        if (waitResult != 0)
        {
            std::vector<JobEntry*>::iterator pos = std::find(jobs.begin(), jobs.end(), job);
            jobs.erase(pos);
        }
    }
}

void JobsList::finishedJobs() {
    SmallShell& shell = SmallShell::getInstance();
    pid_t pid_smash = getpid();

    if (pid_smash < 0) {
        std::perror("smash error: getpid failed");
        return;
    }

    if (pid_smash != shell.getSmashPID()) {
        return;
    }

    for (unsigned int i = 0; i < jobs.size(); i++)
    {
        JobsList::JobEntry* job = jobs[i];
        int waitStatus = waitpid(job->pid, nullptr, WNOHANG);
        if (waitStatus != 0) {
            jobs.erase(jobs.begin() + i);
        }
    }

    if (jobs.empty()) {
        jobCounter = 0;
    }
    else {
        jobCounter = jobs.back()->pid;
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
    for (JobEntry* job : jobs) {
        if (job->job_id == jobId) {
            return job;
        }
    }

    return nullptr;
}

JobsList::JobEntry* JobsList::getJobByPID(int jobPID) {
    for (JobEntry* job : jobs) {
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

    for (JobEntry* job : jobs) {
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
    for (JobEntry* job : jobs) {
        if (job->job_id > maxID) {
            maxID = job->job_id;
        }
    }

    return maxID;
}

JobsList::JobEntry* JobsList::getLastJob(int* lastJobId) {
    if (jobs.empty() || !lastJobId) {
        return nullptr;
    }
    *lastJobId = getMaxJobID();
    return getJobById(*lastJobId);
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId) {
    int maxID = 0;
    JobEntry* lastStoppedJob = nullptr;

    for (JobEntry* job : jobs) {
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
    this->last_dir_path = new char* [FILENAME_MAX];
    *this->last_dir_path = nullptr;
    this->timed_jobs = new TimedJobs();

}

SmallShell::~SmallShell()
{
    delete[] last_dir_path;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

Command* SmallShell::CreateCommand(const char* cmd_line)
{
    if (!cmd_line)
    {
        return nullptr;
    }
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (cmd_s.find('>') != string::npos)
    {
        return new RedirectionCommand(cmd_line);
    }
    else if (cmd_s.find("|&") != string::npos)
    {
        return new PipeCommand(cmd_line, stdErr);
    }
    else if (cmd_s.find('|') != string::npos)
    {
        return new PipeCommand(cmd_line, stdIn);
    }
    else if (firstWord.compare("chprompt") == 0 || firstWord == "chprompt&")
    {
        return new ChangePromptCommand(cmd_line);
    }
    else if (firstWord == "showpid" || firstWord == "showpid&")
    {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord == "pwd" || firstWord == "pwd&")
    {
        return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord == "cd" || firstWord == "cd&")
    {
        return new ChangeDirCommand(cmd_line, last_dir_path);
    }
    else if (firstWord == "jobs" || firstWord == "jobs&")
    {
        return new JobsCommand(cmd_line, &jobs);
    }
    else if (firstWord == "fg" || firstWord == "fg&")
    {
        return new ForegroundCommand(cmd_line, &this->jobs);
    }
    else if (firstWord == "bg" || firstWord == "bg&")
    {
        return new BackgroundCommand(cmd_line, &this->jobs);
    }
    else if (firstWord == "quit" || firstWord == "quit&")
    {
        return new QuitCommand(cmd_line, &this->jobs);
    }
    else if (firstWord == "kill" || firstWord == "kill&")
    {
        return new KillCommand(cmd_line, &this->jobs);
    }
    else if (firstWord == "setcore" || firstWord == "setcore&")
    {
        return new SetcoreCommand(cmd_line);
    }
    else if (firstWord == "getfiletype" || firstWord == "getfiletype&")
    {
        return new GetFileTypeCommand(cmd_line);
    }
    else if (firstWord == "chmod" || firstWord == "chmod&")
    {
        return new ChmodCommand(cmd_line);
    }
    else if (firstWord == "timeout")
    {
        return new TimeoutCommand(cmd_line);
    }
    else
    {
        bool isBg = _isBackgroundComamnd(cmd_line);
        return new ExternalCommand(cmd_line, &jobs, isBg);
    }

    return nullptr;
}

void SmallShell::executeCommand(const char* cmd_line)
{
    this->jobs.removeFinishedJobs();

    Command* cmd = CreateCommand(cmd_line);
    this->currentCommandLine = cmd_line;

    if (cmd_line)
    {
        cmd->execute();
        delete cmd;
    }
    this->currentJobPID = -1;
}
