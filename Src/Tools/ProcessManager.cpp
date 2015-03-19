
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
  // todo: interrupt wait one
  thread.join();
}

bool_t ProcessManager::startProcess(const String& commandLine)
{
  // todo: interrupt wait one
  return false;
}

uint_t ProcessManager::proc(void_t* param)
{
  for(;;)
  {
    //uint32_t id = Process::waitOne();
  }
  return 0;
}
