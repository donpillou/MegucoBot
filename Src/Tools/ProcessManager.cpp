
#include <nstd/Array.h>
#include <nstd/Process.h>
#include <nstd/HashMap.h>
#include <nstd/Log.h>

#include "ProcessManager.h"

bool_t ProcessManager::start(Callback& callback)
{
  this->callback = &callback;
  if(!thread.start(proc, this))
    return false;
  return true;
}

void_t ProcessManager::stop()
{
  mutex.lock();
  Action& action = actions.append(Action());
  action.type = Action::quitType;
  mutex.unlock();
  Process::interrupt();
  thread.join();
}

void_t ProcessManager::startProcess(uint64_t id, const String& commandLine)
{
  mutex.lock();
  Action& action = actions.append(Action());
  action.type = Action::startType;
  action.commandLine = commandLine;
  action.id = id;
  mutex.unlock();
  Process::interrupt();
}

void_t ProcessManager::killProcess(uint64_t id)
{
  mutex.lock();
  Action& action = actions.append(Action());
  action.type = Action::killType;
  action.id = id;
  mutex.unlock();
  Process::interrupt();
}

uint_t ProcessManager::proc()
{
  Array<Process*> processes;
  HashMap<Process*, uint64_t> processIdMap;

  for(;;)
  {
    Process* process = Process::wait(processes, processes.size());
    if(process)
    {
      HashMap<Process*, uint64_t>::Iterator it = processIdMap.find(process);
      if(it != processIdMap.end())
      {
        Log::infof("reaped process %d", process->getProcessId());
        callback->terminatedProcess(*it);
        processIdMap.remove(it);
        Array<Process*>::Iterator it2 = processes.find(process);
        if(it2 != processes.end())
          processes.remove(it2);
        delete process;
      }
    }
    else
    {
      for(;;)
      {
        mutex.lock();
        if(actions.isEmpty())
        {
          mutex.unlock();
          break;
        }
        Action& action = actions.front();
        switch(action.type)
        {
        case Action::quitType:
          actions.removeFront();
          mutex.unlock();
          return 0;
        case Action::startType:
          {
            Process* process = new Process();
            String command = action.commandLine;
            command.resize(command.length());
            if(!process->start(action.commandLine))
            {
              Log::infof("could not launch: %s", (const char_t*)command);
              callback->terminatedProcess(action.id);
              delete process;
            }
            else
            {
              Log::infof("started process %u: %s", process->getProcessId(), (const char_t*)command);
              processes.append(process);
              processIdMap.append(process, action.id);
            }
          }
          break;
        case Action::killType:
          {
            for(HashMap<Process*, uint64_t>::Iterator i = processIdMap.begin(), end = processIdMap.end(); i != end; ++i)
              if(*i == action.id)
              {
                process = i.key();
                Array<Process*>::Iterator it = processes.find(process);
                if(it != processes.end())
                {
                  Process* process = *it;
                  uint32_t pid = process->getProcessId();
                  if(process->kill())
                  {
                    Log::infof("killed process %u", pid);
                    callback->terminatedProcess(action.id);
                    processes.remove(it);
                    processIdMap.remove(i);
                    delete process;
                  }
                  else
                  {
                    Log::infof("could not kill process %u", pid);
                  }
                }
                break;
              }
          }
        }
        actions.removeFront();
        mutex.unlock();
      }
    }
  }
  return 0;
}
