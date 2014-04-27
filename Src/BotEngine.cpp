
#include <nstd/File.h>

#include "BotEngine.h"
#include "BotProtocol.h"
#include "ClientHandler.h"

BotEngine::BotEngine(uint32_t id, const String& path) : id(id), path(path)
{
  name = File::basename(path, ".exe");
}

void_t BotEngine::send(ClientHandler& client)
{
  BotProtocol::BotEngine botEngine;
  BotProtocol::setString(botEngine.name, name);
  client.sendEntity(BotProtocol::botEngine, id, &botEngine, sizeof(botEngine));
}
