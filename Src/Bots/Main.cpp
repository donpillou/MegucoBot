
#include <nstd/Console.h>

#include "Tools/ConnectionHandler.h"

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t botPort = 40124;
  static const uint32_t dataIp = Socket::inetAddr("192.168.0.49");
  static const uint16_t dataPort = 40123;

  //for(;;)
  //{
  //  bool stop = true;
  //  if(!stop)
  //    break;
  //}

  ConnectionHandler connectionHandler;
  if(!connectionHandler.connect(botPort, dataIp, dataPort))
  {
    Console::errorf("error: Could not connect to bot server: %s\n", (const tchar_t*)connectionHandler.getLastError());
    return -1;
  }

  if(!connectionHandler.process())
  {
    Console::errorf("error: %s\n", (const tchar_t*)connectionHandler.getLastError());
    return -1;
  }

  return 0;
}
