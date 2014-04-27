
#include <nstd/File.h>

#include "Engine.h"
#include "BotProtocol.h"
#include "ClientHandler.h"

Engine::Engine(uint32_t id, const String& path) : id(id), path(path)
{
  name = File::basename(path, ".exe");
}

void_t Engine::send(ClientHandler& client)
{
  BotProtocol::Engine engineData;
  BotProtocol::setString(engineData.name, name);
  client.sendEntity(BotProtocol::engine, id, &engineData, sizeof(engineData));
}
