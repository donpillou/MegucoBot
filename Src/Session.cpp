
#include "Session.h"

Session::Session(uint32_t id, const String& name) : id(id), name(name) {}

Session::~Session()
{
  process.kill();
}

bool_t Session::start(const String& engine, double balanceBase, double balanceComm)
{
  if(process.isRunning())
    return false;
  if(!process.start(engine))
    return false;
  return true;
}
