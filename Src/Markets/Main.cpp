
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

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t port = 40124;

  BotConnection connection;
  if(!connection.connect(port))
  {
    Console::errorf("error: Could not connect to bot server: %s\n", (const char_t*)connection.getErrorString());
    return -1;
  }


  return 0;
}
