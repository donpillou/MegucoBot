
#include <nstd/Array.h>
#include <nstd/Process.h>
#include <nstd/HashMap.h>
#include <nstd/Console.h>

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

bool_t ProcessManager::startProcess(const String& commandLine, uint32_t& id)
{
  //Console::printf("launching: %s\n", (const char_t*)commandLine);
  id = nextId++;
  mutex.lock();
  Action& action = actions.append(Action());
  action.type = Action::startType;
  action.commandLine = commandLine;
  action.id = id;
  mutex.unlock();
  Process::interrupt();
  return true;
}

bool_t ProcessManager::killProcess(uint32_t id)
{
  mutex.lock();
  Action& action = actions.append(Action());
  action.type = Action::killType;
  action.id = id;
  mutex.unlock();
  Process::interrupt();
  return true;
}

uint_t ProcessManager::proc()
{
  Array<Process*> processes;
  HashMap<Process*, uint32_t> processIdMap;

  for(;;)
  {
    Process* process = Process::wait(processes, processes.size());
    if(process)
    {
      uint32_t id = *processIdMap.find(process);
      callback->terminatedProcess(id);
      Array<Process*>::Iterator it = processes.find(process);
      if(it != processes.end())
      {
        processIdMap.remove(*it);
        processes.remove(it);
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
              callback->terminatedProcess(action.id);
              delete process;
            }
            else
            {
              processes.append(process);
              processIdMap.append(process, action.id);
            }
          }
          break;
        case Action::killType:
          {
            HashMap<Process*, uint32_t>::Iterator itMap = processIdMap.end();
            for(HashMap<Process*, uint32_t>::Iterator end = processIdMap.end(); itMap != end; ++itMap)
              if(*itMap == action.id)
                break;
            if(itMap != processIdMap.end())
            {
              process = itMap.key();
              Array<Process*>::Iterator it = processes.find(process);
              if(it != processes.end())
              {
                Process* process = *it;
                if(process->kill())
                {
                  callback->terminatedProcess(action.id);
                  processes.remove(it);
                  processIdMap.remove(itMap);
                  delete process;
                }
              }
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