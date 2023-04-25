#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num)
{
	SmallShell& shell = SmallShell::getInstance();
    std::cout << "smash: got ctrl-Z" << std::endl;

    if(shell.getCurrentJobPID() != -1)
    {
        if (killpg(shell.getCurrentJobPID(), SIGTSTP) == -1)
        {
            std::perror("smash error: killpg failed");
            return;
        }

        JobsList::JobEntry* currentJob = shell.jobs.getJobByPID(shell.getCurrentJobPID());

        if(currentJob)
        {
            currentJob->elapsed_time = time(nullptr);
            currentJob->is_stopped = true;
        }
        else
        {
            Command* cmd = shell.CreateCommand(currentJob->command->cmdLine.c_str());
            shell.jobs.addJob(cmd, shell.getCurrentJobPID(), true);
        }

        std::cout << "smash: process " << currentJob->pid << " was stopped" << std::endl;
        shell.setCurrentJobPID(-1);
    }
}

void ctrlCHandler(int sig_num)
{
    SmallShell& shell = SmallShell::getInstance();
    std::cout << "smash: got ctrl-C" << std::endl;

    pid_t currentRunningJobPID = shell.getCurrentJobPID();

    if (currentRunningJobPID != -1)
    {
        if (killpg(currentRunningJobPID, SIGKILL) == -1)
        {
            std::perror("smash error: killpg failed");
            return;
        }

        JobsList::JobEntry* job = shell.jobs.getJobById(currentRunningJobPID);

        if(job != nullptr)
        {
            shell.jobs.removeJobById(job->job_id);
        }

        std::cout << "smash: process " << job->pid << " was killed" << std::endl;
        shell.setCurrentJobPID(-1);
    }
}

void alarmHandler(int sig_num)
{
    SmallShell& shell = SmallShell::getInstance();
    cout << "smash: got an alarm" << endl;

    TimedEntry* job_to_kill = shell.timed_jobs->timeouts[0];

    pid_t pid_to_kill = job_to_kill->getPid();

    if(pid_to_kill == shell.getSmashPID())
    {
        return;
    }

    if (killpg(pid_to_kill, SIGKILL) == -1)
    {
        std::perror("smash error: killpg failed");
        return;
    }

    cout << "smash: timeout " << job_to_kill->getTimer() << " " << job_to_kill->getCommandLine() << " timed out!" << endl;
    shell.timed_jobs->removeKilledJobs();
    shell.jobs.finishedJobs();

    if(shell.timed_jobs->timeouts.empty())
    {
        return;
    }

    TimedEntry* curr_alarmed_job = shell.timed_jobs->timeouts[0];
    time_t curr_alarm = curr_alarmed_job->getEndTime();

    alarm(curr_alarm-time(nullptr));
}

