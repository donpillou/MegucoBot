
#pragma once

#include <nstd/HashMap.h>
#include <nstd/Buffer.h>

#include "Tools/Broker.h"

class ConnectionHandler;

class LiveBroker : public Broker
{
public:
  LiveBroker(ConnectionHandler& connectionHandler, const String& currencyBase, const String& currencyComm, double tradeFree, timestamp_t maxTradeAge);

private:
  ConnectionHandler& connectionHandler;
  String currencyBase;
  String currencyComm;
  String error;
  HashMap<uint64_t, meguco_user_market_order_entity> openOrders;
  timestamp_t time;
  timestamp_t lastBuyTime;
  timestamp_t lastSellTime;
  timestamp_t lastOrderRefreshTime;
  HashMap<uint64_t, meguco_user_market_transaction_entity> transactions;
  HashMap<uint64_t, meguco_user_session_asset_entity> assets;
  HashMap<String, Buffer> properties;

private:
  void_t refreshOrders(Bot::Session& botSession);
  void_t cancelTimedOutOrders(Bot::Session& botSession);
  void_t setProperty(const String& name, const String& value, meguco_user_session_property_type type, uint32_t flags, const String& unit);

private: // Bot::Broker
  virtual const String& getCurrencyBase() const {return currencyBase;};
  virtual const String& getCurrencyComm() const {return currencyComm;};

  virtual bool_t buy(double price, double amount, double total, timestamp_t timeout, uint64_t* id, double* orderedAmount);
  virtual bool_t sell(double price, double amount, double total, timestamp_t timeout, uint64_t* id, double* orderedAmount);
  virtual bool_t cancelOder(uint64_t id);
  virtual const HashMap<uint64_t, meguco_user_market_order_entity> getOrders() const {return openOrders;}
  virtual const meguco_user_market_order_entity* getOrder(uint64_t id) const;
  virtual size_t getOpenBuyOrderCount() const;
  virtual size_t getOpenSellOrderCount() const;
  virtual timestamp_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual timestamp_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual const HashMap<uint64_t, meguco_user_session_asset_entity>& getAssets() const {return assets;}
  virtual const meguco_user_session_asset_entity* getAsset(uint64_t id) const;
  virtual bool_t createAsset(meguco_user_session_asset_entity& asset);
  virtual void_t removeAsset(uint64_t id);
  virtual void_t updateAsset(const meguco_user_session_asset_entity& asset);

  //virtual const HashMap<String, BotProtocol::SessionProperty>& getProperties() const {return properties;}
  //virtual const BotProtocol::SessionProperty* getProperty(uint64_t id) const;
  //virtual void_t updateProperty(const BotProtocol::SessionProperty& property);
  virtual double getProperty(const String& name, double defaultValue) const;
  virtual String getProperty(const String& name, const String& defaultValue) const;
  virtual void_t registerProperty(const String& name, double value, uint32_t flags = meguco_user_session_property_none, const String& unit = String());
  virtual void_t registerProperty(const String& name, const String& value, uint32_t flags = meguco_user_session_property_none, const String& unit = String());
  virtual void_t setProperty(const String& name, double value, uint32_t flags, const String& unit);
  virtual void_t setProperty(const String& name, const String& value, uint32_t flags, const String& unit);
  virtual void_t removeProperty(const String& name);

  virtual void_t addMarker(meguco_user_session_marker_type markerType);
  virtual void_t warning(const String& message);

public: // Broker
  virtual const String& getLastError() const {return error;}
  virtual void_t handleTrade(Bot::Session& session, const meguco_trade_entity& trade, bool_t replayed);
};

