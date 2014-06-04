
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#endif

#include <nstd/Console.h>
#include <nstd/Debug.h>
#include <nstd/Directory.h>
#include <nstd/File.h>
#include <nstd/Error.h>

#include "Tools/Server.h"
#include "ServerHandler.h"

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t port = 40124;
  bool background = true;
  String dataDir("Data");
#ifdef _DEBUG
  String defaultBinaryDir("Build/Debug");
#else
  String defaultBinaryDir("Build/Release");
#endif
  String binaryDir = defaultBinaryDir;

  // parse parameters
  for(int i = 1; i < argc; ++i)
    if(String::compare(argv[i], "-f") == 0)
      background = false;
    else if(String::compare(argv[i], "-c") == 0&& i + 1 < argc)
    {
      ++i;
      dataDir = String(argv[i], String::length(argv[i]));
    }
    else if(String::compare(argv[i], "-b") == 0&& i + 1 < argc)
    {
      ++i;
      binaryDir = String(argv[i], String::length(argv[i]));
    }
    else
    {
      Console::errorf("Usage: %s [-b] [-b <dir>]\n\
  -f            run in foreground (not as daemon)\n\
  -c <dir>      set data directory (default is ./Data)\n\
  -b <dir>      set binary directory (default is ./%s)\n", argv[0], (const char_t*)defaultBinaryDir);
      return -1;
    }

  // change working directory
  if(!Directory::exists(dataDir) && !Directory::create(dataDir))
  {
    Console::errorf("error: Could not create data directory: %s\n", (const tchar_t*)Error::getErrorString());
    return -1;
  }
  if(!File::isAbsolutePath(binaryDir))
    binaryDir = Directory::getCurrent() + "/" + binaryDir;
  if(!dataDir.isEmpty() && !Directory::change(dataDir))
  {
    Console::errorf("error: Could not enter data directory: %s\n", (const tchar_t*)Error::getErrorString());
    return -1;
  }
  binaryDir = File::getRelativePath(Directory::getCurrent(), binaryDir);

#ifndef _WIN32
  // daemonize process
  if(background)
  {
    Console::printf("Starting as daemon...\n");

    int fd = open("traded.log", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if(fd == -1)
    {
      Console::errorf("error: Could not open file %s: %s\n", "traded.log", strerror(errno));
      return -1;
    }
    if(dup2(fd, STDOUT_FILENO) == -1)
    {
      Console::errorf("error: Could not reopen stdout: %s\n", strerror(errno));
      return 0;
    }
    if(dup2(fd, STDERR_FILENO) == -1)
    {
      Console::errorf("error: Could not reopen stdout: %s\n", strerror(errno));
      return 0;
    }
    close(fd);

    pid_t childPid = fork();
    if(childPid == -1)
      return -1;
    if(childPid != 0)
      return 0;
  }
#endif

  Console::printf("Starting bot server...\n", port);

  // initialize listen server
  Server server;
  ServerHandler serverHandler(port);
  server.setListener(&serverHandler);

  // load market list
#ifdef _WIN32
  serverHandler.addMarketAdapter("Bitstamp/USD", "BitstampUsd.exe", "USD", "BTC");
#else
  serverHandler.addMarketAdapter("Bitstamp/USD", "BitstampUsd", "USD", "BTC");
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
          if(name.endsWith("Bot"))
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
  return 0;
}

