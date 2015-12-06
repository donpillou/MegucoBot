
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

void_t ProcessManager::setProcessId(uint64_t id, uint64_t newId)
{
  if(id == newId)
    return;
  mutex.lock();
  HashMap<uint64_t, Process*>::Iterator it = processesByIdMap.find(id);
  if(it == processesByIdMap.end())
  {
    mutex.unlock();
    return;
  }
  Process* process = *it;
  idsByProcessMap.append(process, newId);
  processesByIdMap.append(newId, process);
  processesByIdMap.remove(it);
  mutex.unlock();
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
  for(;;)
  {
    Process* process = Process::wait(processes, processes.size());
    if(process)
    {
      HashMap<Process*, uint64_t>::Iterator it = idsByProcessMap.find(process);
      if(it != idsByProcessMap.end())
      {
        Log::infof("reaped process %d", process->getProcessId());
        uint64_t processEntityId = *it;
        callback->terminatedProcess(processEntityId);
        idsByProcessMap.remove(it);
        processesByIdMap.remove(processEntityId);
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
            if(!process->start(action.commandLine))
            {
              Log::infof("could not launch: %s", (const char_t*)action.commandLine);
              callback->terminatedProcess(action.id);
              delete process;
            }
            else
            {
              Log::infof("started process %u: %s", process->getProcessId(), (const char_t*)action.commandLine);
              processes.append(process);
              idsByProcessMap.append(process, action.id);
              processesByIdMap.append(action.id, process);
            }
          }
          break;
        case Action::killType:
          {
            HashMap<uint64_t, Process*>::Iterator it = processesByIdMap.find(action.id);
            if(it == processesByIdMap.end())
              break;
            process = *it;
            Array<Process*>::Iterator it2 = processes.find(process);
            if(it2 != processes.end())
            {
              uint32_t pid = process->getProcessId();
              if(process->kill())
              {
                Log::infof("killed process %u", pid);
                callback->terminatedProcess(action.id);
                processes.remove(it2);
                processesByIdMap.remove(it);
                idsByProcessMap.remove(process);
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
        actions.removeFront();
        mutex.unlock();
      }
    }
  }
  return 0;
}
