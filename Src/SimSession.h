

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>

class SimSession
{
public:
  SimSession(uint32_t id, const String& name);
  ~SimSession();

  bool_t start(const String& engine, double balanceBase, double balanceComm);

  const String& getEngineName() const {return engine;}

private:
  uint32_t id;
  String name;
  Process process;
  String engine;
};
