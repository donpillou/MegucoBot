
#include <nstd/Console.h>
#include <nstd/File.h>
#include <nstd/Thread.h>

#include "ConnectionHandler.h"

int_t main(int_t argc, char_t* argv[])
{
  String binaryDir = File::dirname(String(argv[0], String::length(argv[0])));

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

