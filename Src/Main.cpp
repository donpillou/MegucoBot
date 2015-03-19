
#include <nstd/Console.h>
#include <nstd/Directory.h>
#include <nstd/File.h>
#include <nstd/Thread.h>
#include <nstd/Process.h>
#include <nstd/Error.h>

#include "ConnectionHandler.h"

int_t main(int_t argc, char_t* argv[])
{
  String logFile;
  String binaryDir = File::dirname(String(argv[0], String::length(argv[0])));

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

  // initialize connection handler
  ConnectionHandler connectionHandler;

  // load market list
  connectionHandler.addBotMarket("Bitstamp/BTC/USD", binaryDir + "/BitstampBtcUsd");

  // load bots
  connectionHandler.addBotEngine("BetBot", binaryDir + "/BetBot");
  connectionHandler.addBotEngine("BetBot2", binaryDir + "/BetBot2");
  connectionHandler.addBotEngine("FlipBot", binaryDir + "/FlipBot");
  connectionHandler.addBotEngine("TestBot", binaryDir + "/TestBot");

  // main loop
  for(;; Thread::sleep(10 * 1000))
  {
    // connect to zlimdb server
    if(!connectionHandler.connect())
    {
        Console::errorf("error: Could not connect to zlimdb server: %s\n", (const char_t*)connectionHandler.getErrorString());
        return -1;
    }
    Console::printf("Connected to zlimdb server.\n");

    // run connection handler loop
    connectionHandler.process();

    Console::errorf("error: Lost connection to zlimdb server: %s\n", (const char_t*)connectionHandler.getErrorString());
  }
  return 0;
}

