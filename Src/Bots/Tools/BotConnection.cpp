
#include <nstd/Process.h>

#include "BotConnection.h"

bool_t BotConnection::connect(uint16_t port)
{
  close();

  // connect to server
  if(!socket.open() ||
      !socket.connect(Socket::loopbackAddr, port) ||
      !socket.setNoDelay())
  {
    error = Socket::getLastErrorString();
    socket.close();
    return false;
  }

  // send register source request
  {
    byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::RegisterBotRequest)];
    BotProtocol::Header* header = (BotProtocol::Header*)message;
    BotProtocol::RegisterBotRequest* registerBotRequest = (BotProtocol::RegisterBotRequest*)(header + 1);
    header->size = sizeof(message);
    header->entityType = BotProtocol::registerBotRequest;
    registerBotRequest->pid = Process::getCurrentProcessId();
    if(!socket.send(message, sizeof(message)))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
  }

  // receive register source response
  {
    BotProtocol::Header header;
    BotProtocol::RegisterBotResponse response;
    if(!socket.recv((byte_t*)&header, sizeof(header)))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
    if(header.entityType != BotProtocol::registerBotResponse)
    {
      error = "Could not receive register bot response.";
      socket.close();
      return false;
    }
    if(!socket.recv((byte_t*)&response, sizeof(response)))
    {
      error = Socket::getLastErrorString();
      socket.close();
      return false;
    }
  }

  return true;
}
