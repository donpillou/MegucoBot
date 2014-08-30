
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Broker.h"
#include "Tools/TradeHandler.h"

class BotConnection;

class SimBroker : public Broker
{
public:
  SimBroker(BotConnection& botConnection, const String& currencyBase, const String& currencyComm, double tradeFree, const BotProtocol::Balance& balance, const List<BotProtocol::Transaction>& transactions, const List<BotProtocol::SessionAsset>& assets, const List<BotProtocol::Order>& orders, const List<BotProtocol::SessionProperty>& properties);

private:
  BotConnection& botConnection;
  String currencyBase;
  String currencyComm;
  String error;
  HashMap<uint32_t, BotProtocol::Order> openOrders;
  timestamp_t time;
  timestamp_t lastBuyTime;
  timestamp_t lastSellTime;
  double tradeFee;
  HashMap<uint32_t, BotProtocol::Transaction> transactions;
  HashMap<uint32_t, BotProtocol::SessionAsset> assets;
  HashMap<String, BotProtocol::SessionProperty> properties;
  TradeHandler tradeHandler;
  timestamp_t startTime;

private: // Bot::Broker
  virtual const String& getCurrencyBase() const {return currencyBase;};
  virtual const String& getCurrencyComm() const {return currencyComm;};

  virtual bool_t buy(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount);
  virtual bool_t sell(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount);
  virtual bool_t cancelOder(uint32_t id);
  virtual const HashMap<uint32_t, BotProtocol::Order> getOrders() const {return openOrders;}
  virtual size_t getOpenBuyOrderCount() const;
  virtual size_t getOpenSellOrderCount() const;
  virtual timestamp_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual timestamp_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual const HashMap<uint32_t, BotProtocol::SessionAsset>& getAssets() const {return assets;}
  virtual const BotProtocol::SessionAsset* getAsset(uint32_t id) const;
  virtual bool_t createAsset(BotProtocol::SessionAsset& asset);
  virtual void_t removeAsset(uint32_t id);
  virtual void_t updateAsset(const BotProtocol::SessionAsset& asset);

  virtual const HashMap<String, BotProtocol::SessionProperty>& getProperties() const {return properties;}
  virtual const BotProtocol::SessionProperty* getProperty(uint32_t id) const;
  virtual void_t updateProperty(const BotProtocol::SessionProperty& property);
  virtual double getProperty(const String& name, double defaultValue) const;
  virtual String getProperty(const String& name, const String& defaultValue) const;
  virtual void_t registerProperty(const String& name, double value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String());
  virtual void_t registerProperty(const String& name, const String& value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String());
  virtual void_t setProperty(const String& name, double value, uint32_t flags, const String& unit);
  virtual void_t setProperty(const String& name, const String& value, uint32_t flags, const String& unit);
  virtual void_t removeProperty(const String& name);

  virtual void_t addMarker(BotProtocol::Marker::Type markerType);
  virtual void_t warning(const String& message);

public: // Broker
  virtual const String& getLastError() const {return error;}
  virtual void_t handleTrade(Bot::Session& session, const DataProtocol::Trade& trade);
};

