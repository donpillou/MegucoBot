
#include <nstd/Console.h>
#include <nstd/Debug.h>
#include <nstd/Directory.h>
#include <nstd/File.h>
#include <nstd/Error.h>
#include <nstd/Process.h>

#include "Tools/Server.h"
#include "ServerHandler.h"

int_t main(int_t argc, char_t* argv[])
{
  String logFile;

  // parse parameters
  {
    Process::Option options[] = {
        {'b', "daemon", Process::argumentFlag | Process::optionalFlag},
        {'h', "help", Process::optionFlag},
    };
    Process::Arguments arguments(argc, argv, options);
    int_t character;
    String argument;
    while(arguments.read(character, argument))
      switch(character)
      {
      case 'b':
        logFile = argument.isEmpty() ? String("MegucoBot.log") : argument;
        break;
      case '?':
        Console::errorf("Unknown option: %s.\n", (const char_t*)argument);
        return -1;
      case ':':
        Console::errorf("Option %s required an argument.\n", (const char_t*)argument);
        return -1;
      default:
        Console::errorf("Usage: %s [-b]\n\
  -b, --daemon[=<file>]   Detach from calling shell and write output to <file>.\n", argv[0]);
        return -1;
      }
  }

  // daemonize process
#ifndef _WIN32
  if(!logFile.isEmpty())
  {
    Console::printf("Starting as daemon...\n");
    if(!Process::daemonize(logFile))
    {
      Console::errorf("error: Could not daemonize process: %s\n", (const char_t*)Error::getErrorString());
      return -1;
    }
  }
#endif

#if 0
  // initialize listen server
  Server server;
  ServerHandler serverHandler(port);
  server.setListener(&serverHandler);

  // load market list
#ifdef _WIN32
  serverHandler.addMarketAdapter("Bitstamp BTC/USD", binaryDir + "/BitstampBtcUsd.exe", "USD", "BTC");
#else
  serverHandler.addMarketAdapter("Bitstamp BTC/USD", binaryDir + "/BitstampBtcUsd", "USD", "BTC");
#endif

  // load bot engine list
  {
    Directory dir;
#ifdef _WIN32
    String pattern("*.exe");
#else
    String pattern;
#endif
    String cd = Directory::getCurrent();
    if(dir.open(binaryDir, pattern, false))
    {
      String path;
      bool_t isDir;
      while(dir.read(path, isDir))
        if(!isDir && path != 
#ifdef _WIN32
          "MegucoBot.exe"
#else
          "MegucoBot"
#endif
          )
        {
          String name = File::basename(path, ".exe");
          if(name.endsWith("Bot") || name.endsWith("Bot2"))
            serverHandler.addBotEngine(name, binaryDir + "/" + path);
        }
    }
  }

  // start listen server
  if(!server.listen(port))
  {
    Console::errorf("error: Could not listen on port %hu: %s\n", port, (const char_t*)Socket::getLastErrorString());
    return -1;
  }

  // load users
  //serverHandler.addUser("donpillou", "1234");
  serverHandler.loadData();

  Console::printf("Listening on port %hu.\n", port);

  if(!server.process())
  {
    Console::errorf("error: Could not run select loop: %s\n", (const char_t*)Socket::getLastErrorString());
    return -1;
  }
#endif
  return 0;
}

