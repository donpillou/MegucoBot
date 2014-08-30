
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>
#include <nstd/Buffer.h>

#include "Tools/Socket.h"
#include "BotProtocol.h"

class BotConnection
{
public:
  BotConnection() : sessionId(0), marketId(0) {}

  bool_t connect(uint16_t port);
  void_t close() {socket.close();}

  const String& getErrorString() const {return error;}

  bool_t getMarketBalance(BotProtocol::Balance& balance);
  bool_t getMarketOrders(List<BotProtocol::Order>& orders);
  bool_t createMarketOrder(BotProtocol::Order& order);
  bool_t removeMarketOrder(uint32_t id);

  bool_t addLogMessage(timestamp_t time, const String& message);

  bool_t getSessionTransactions(List<BotProtocol::Transaction>& transactions);
  bool_t getSessionAssets(List<BotProtocol::SessionAsset>& assets);
  bool_t getSessionOrders(List<BotProtocol::Order>& orders);
  bool_t getSessionProperties(List<BotProtocol::SessionProperty>& properties);

  bool_t createSessionTransaction(BotProtocol::Transaction& transaction);
  bool_t updateSessionTransaction(const BotProtocol::Transaction& transaction);
  bool_t removeSessionTransaction(uint32_t id);
  bool_t createSessionAsset(BotProtocol::SessionAsset& asset);
  bool_t updateSessionAsset(const BotProtocol::SessionAsset& asset);
  bool_t removeSessionAsset(uint32_t id);
  bool_t createSessionOrder(BotProtocol::Order& order);
  bool_t updateSessionOrder(BotProtocol::Order& order);
  bool_t removeSessionOrder(uint32_t id);
  bool_t createSessionMarker(BotProtocol::Marker& marker);
  bool_t removeSessionMarker(uint32_t id);
  bool_t createSessionProperty(BotProtocol::SessionProperty& property) {return createEntity(&property, sizeof(property));}
  bool_t updateSessionProperty(const BotProtocol::SessionProperty& property) {return updateEntity(&property, sizeof(property));}
  bool_t removeSessionProperty(uint32_t id) {return removeEntity(BotProtocol::sessionProperty, id);}

private:
  Socket socket;
  String error;
  Buffer recvBuffer;

  uint32_t sessionId;
  uint32_t marketId;

private:
  bool_t createEntity(void_t* data, size_t size);
  bool_t updateEntity(const void_t* data, size_t size);
  bool_t removeEntity(uint32_t type, uint32_t id);
  //bool_t sendPing();
  bool_t sendControlSession(BotProtocol::ControlSession::Command cmd);
  template<class E> bool_t sendControlMarket(BotProtocol::ControlMarket::Command cmd, List<E>& result);
  template<class E> bool_t sendControlSession(BotProtocol::ControlSession::Command cmd, List<E>& result);
  bool_t sendMessage(BotProtocol::MessageType type, uint32_t requestId, const void_t* data, size_t size);
  bool_t receiveMessage(BotProtocol::Header& header, byte_t*& data, size_t& size);
};
