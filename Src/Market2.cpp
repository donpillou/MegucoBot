
#include "Market2.h"

#include "Tools/ProcessManager.h"

bool_t Market2::startProcess()
{
  if(id)
    return false;
  if(!processManager.startProcess(executable, id))
    return false;
  return true;
}
