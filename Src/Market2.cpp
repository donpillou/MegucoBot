
#include "Market2.h"

#include "Tools/ProcessManager.h"

bool_t Market2::startProcess()
{
  if(!processManager.startProcess(executable))
    return false;
  return true;
}
