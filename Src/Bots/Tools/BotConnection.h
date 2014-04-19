
#pragma once

#include <nstd/String.h>
#include "Tools/Socket.h"
//#include "Tools/Market.h"
#include "BotProtocol.h"

class BotConnection
{
public:
  BotConnection() {}

  bool_t connect(uint16_t port);
  void_t close() {socket.close();}
  bool_t isOpen() const {return socket.isOpen();}
  const String& getErrorString() const {return error;}

  bool_t createTransaction(const BotProtocol::CreateTransactionArgs& transaction);
  bool_t createOrder(const BotProtocol::CreateOrderArgs& order);

private:
  Socket socket;
  String error;

private:
  bool_t createEntity(BotProtocol::EntityType type, const void_t* data, size_t size);
};
