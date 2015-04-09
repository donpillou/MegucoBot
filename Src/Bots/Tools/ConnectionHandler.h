
#pragma once

#include "Tools/ZlimdbConnection.h"

#include "Broker.h"

class ConnectionHandler : public ZlimdbConnection::Callback
{
public:
  ConnectionHandler() : broker(0), botSession(0), lastReceivedTradeId(0) {}
  ~ConnectionHandler() {delete broker;}

  bool_t connect2(uint32_t sessionTableId, bool_t simulation);
  const String& getLastError() {return error;}

  bool_t process();

  bool_t getMarketBalance(meguco_user_market_balance_entity& balance);
  bool_t getMarketOrders(List<meguco_user_market_order_entity>& orders);
  bool_t createMarketOrder(meguco_user_market_order_entity& order);
  bool_t removeMarketOrder(uint64_t id);

  bool_t createSessionTransaction(meguco_user_market_transaction_entity& transaction);
  bool_t createSessionOrder(meguco_user_market_order_entity& order);
  bool_t removeSessionOrder(uint64_t id);
  bool_t createSessionAsset(meguco_user_session_asset_entity& asset);
  bool_t updateSessionAsset(const meguco_user_session_asset_entity& asset);
  bool_t removeSessionAsset(uint64_t id);
  bool_t createSessionMarker(meguco_user_session_marker_entity& marker);
  bool_t createSessionProperty(meguco_user_session_property_entity& property);
  bool_t updateSessionProperty(const meguco_user_session_property_entity& property);
  bool_t removeSessionProperty(uint64_t id);

  bool_t addLogMessage(timestamp_t time, const String& message);

private:
  //uint32_t dataIp;
  //uint16_t dataPort;
  String error;
  ZlimdbConnection connection;
  //BotConnection botConnection;
  //HandlerConnection handlerConnection;
  //DataConnection dataConnection;
  timestamp_t maxTradeAge;
  Broker* broker;
  Bot::Session* botSession;

  //Socket* sessionHandlerSocket;

  uint64_t lastReceivedTradeId;

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId);
  virtual void_t controlEntity(uint32_t tableId, uint64_t entityId, uint32_t controlCode, const Buffer& buffer);

//private:
//  void_t handleCreateEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size);
//  void_t handleUpdateEntity(uint32_t requestId, const BotProtocol::Entity& entity, size_t size);
//  void_t handleRemoveEntity(uint32_t requestId, const BotProtocol::Entity& entity);
//  void_t handleControlEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size);
//
//  void_t handleCreateSessionAsset(uint32_t requestId, BotProtocol::SessionAsset& sessionAsset);
//  void_t handleUpdateSessionAsset(uint32_t requestId, const BotProtocol::SessionAsset& sessionAsset);
//  void_t handleRemoveSessionAsset(uint32_t requestId, const BotProtocol::Entity& entity);
//
//  void_t handleUpdateSessionProperty(uint32_t requestId, BotProtocol::SessionProperty& sessionProperty);

//private: // HandlerConnection::Callback
//  virtual void_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size);
//
//private: // DataConnection::Callback
//  virtual void_t receivedChannelInfo(const String& channelName) {}
//  virtual void_t receivedSubscribeResponse(const String& channelName, uint64_t channelId) {}
//  virtual void_t receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) {}
//  virtual void_t receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade);
//  virtual void_t receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) {}
//  virtual void_t receivedErrorResponse(const String& message) {}
};
