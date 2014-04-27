
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#endif

#include <nstd/Console.h>
#include <nstd/String.h>
#include <nstd/Thread.h> // sleep
//#include <nstd/Error.h>

#include "Tools/BotConnection.h"

//#ifdef BOT_BUYBOT
//#include "Bots/BuyBot.h"
//typedef BuyBot MarketConnection;
//const char* botName = "BuyBot";
//#endif

bool_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size);
bool_t handelRequestEntites(BotProtocol::EntityType entityType);

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t port = 40124;

  // create connection to bot server
  BotConnection connection;
  if(!connection.connect(port))
  {
    Console::errorf("error: Could not connect to bot server: %s\n", (const char_t*)connection.getErrorString());
    return -1;
  }

  // wait for requests
  BotProtocol::Header header;
  byte_t* data;
  for(;;)
  {
    if(!connection.receiveMessage(header, data))
    {
      Console::errorf("error: Lost connection to bot server: %s\n", (const char_t*)connection.getErrorString());
      return -1;
    }

    // handle message
    if(!handleMessage(header, data, header.size - sizeof(header)))
    {
      Console::errorf("error: Lost connection to bot server: %s\n", (const char_t*)connection.getErrorString());
      return -1;
    }
  }
  return 0;
}

bool_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size)
{
  switch((BotProtocol::MessageType)header.messageType)
  {
  case BotProtocol::requestEntities:
    if(size >= sizeof(BotProtocol::Entity))
      return handelRequestEntites((BotProtocol::EntityType)((BotProtocol::Entity*)data)->entityType);
  default:
    break;
  }
  return true;
}

bool_t handelRequestEntites(BotProtocol::EntityType entityType)
{
  switch(entityType)
  {
  case BotProtocol::marketTransaction:
    break;
  default:
    break;
  }
  return true;
}
