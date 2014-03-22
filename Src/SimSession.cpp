
#include "SimSession.h"

SimSession::SimSession(uint32_t id, const String& name) : id(id), name(name) {}

bool_t SimSession::start(const String& engine, double balanceBase, double balanceComm)
{
  if(process.isRunning())
    return false;
  String commandLine;
  commandLine.printf("%s --id %lld", (const char*)engine, id);
  if(!process.start(engine))
    return false;
  return true;
}
