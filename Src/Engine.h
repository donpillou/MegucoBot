
#pragma once

#include <nstd/String.h>

class Engine
{
public:

public:
  Engine(uint32_t id, const String& path);

  uint32_t getId() const {return id;}
  const String& getName() const {return name;}
  const String& getPath() const {return path;}

private:
  uint32_t id;
  String name;
  String path;
};
