

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>

class Session
{
public:
  Session(uint32_t id, const String& name);
  ~Session();

  bool_t start(const String& engine, double balanceBase, double balanceComm);

private:
  uint32_t id;
  String name;
  Process process;
};
