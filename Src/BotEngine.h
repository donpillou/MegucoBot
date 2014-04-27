
#pragma once

#include <nstd/String.h>

class ClientHandler;

class BotEngine
{
public:

public:
  BotEngine(uint32_t id, const String& path);

  uint32_t getId() const {return id;}
  const String& getName() const {return name;}
  const String& getPath() const {return path;}

  void_t send(ClientHandler& client);

private:
  uint32_t id;
  String name;
  String path;
};
