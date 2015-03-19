
#pragma once

#include <nstd/String.h>
#include <nstd/Thread.h>

class ProcessManager
{
public:
  class Callback
  {
  public:
    virtual void processTerminated(uint32_t pid) = 0;
  };

public:
  bool_t start(Callback& callback);
  void_t stop();

  bool_t startProcess(const String& commandLine);

private:
  Thread thread;
  Callback* callback;

private:
  static uint_t proc(void_t* param);
};
