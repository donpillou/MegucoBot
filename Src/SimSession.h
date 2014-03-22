

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>

class SimSession
{
public:
  SimSession(uint64_t id, const String& name);

  bool_t start(const String& engine, double balanceBase, double balanceComm);

private:
  uint64_t id;
  String name;
  Process process;
};
