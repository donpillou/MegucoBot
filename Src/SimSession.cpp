
#include "SimSession.h"

SimSession::SimSession(uint32_t id, const String& name) : id(id), name(name) {}

SimSession::~SimSession()
{
  process.kill();
}

bool_t SimSession::start(const String& engine, double balanceBase, double balanceComm)
{
  if(process.isRunning())
    return false;
  if(!process.start(engine))
    return false;
  this->engine = engine;
  return true;
}
