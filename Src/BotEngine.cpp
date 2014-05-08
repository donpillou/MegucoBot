
#include <nstd/File.h>

#include "BotEngine.h"
#include "BotProtocol.h"
#include "ClientHandler.h"

BotEngine::BotEngine(uint32_t id, const String& path) : __id(id), path(path)
{
  name = File::basename(path, ".exe");
}

void_t BotEngine::send(ClientHandler& client)
{
  BotProtocol::BotEngine botEngine;
  botEngine.entityType = BotProtocol::botEngine;
  botEngine.entityId = __id;
  BotProtocol::setString(botEngine.name, name);
  client.sendEntity(&botEngine, sizeof(botEngine));
}
