
#pragma once

#include <nstd/HashMap.h>

#include "Tools/Broker.h"
#include "Tools/TradeHandler.h"

class BotConnection;

class LiveBroker : public Broker
{
public:
  LiveBroker(BotConnection& botConnection, const String& currencyBase, const String& currencyComm, const BotProtocol::Balance& balance, const List<BotProtocol::Transaction>& transactions, const List<BotProtocol::SessionItem>& items, const List<BotProtocol::Order>& orders, const List<BotProtocol::SessionProperty>& properties);

private:
  BotConnection& botConnection;
  String currencyBase;
  String currencyComm;
  String error;
  List<BotProtocol::Order> openOrders;
  timestamp_t time;
  timestamp_t lastBuyTime;
  timestamp_t lastSellTime;
  timestamp_t lastOrderRefreshTime;
  HashMap<uint32_t, BotProtocol::Transaction> transactions;
  HashMap<uint32_t, BotProtocol::SessionItem> items;
  HashMap<String, BotProtocol::SessionProperty> properties;
  TradeHandler tradeHandler;

private:
  void_t refreshOrders(Bot::Session& botSession);
  void_t cancelTimedOutOrders(Bot::Session& botSession);

private: // Bot::Broker
  virtual const String& getCurrencyBase() const {return currencyBase;};
  virtual const String& getCurrencyComm() const {return currencyComm;};

  virtual bool_t buy(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount);
  virtual bool_t sell(double price, double amount, double total, timestamp_t timeout, uint32_t* id, double* orderedAmount);
  virtual size_t getOpenBuyOrderCount() const;
  virtual size_t getOpenSellOrderCount() const;
  virtual timestamp_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual timestamp_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual const HashMap<uint32_t, BotProtocol::SessionItem>& getItems() const {return items;}
  virtual const BotProtocol::SessionItem* getItem(uint32_t id) const;
  virtual bool_t createItem(BotProtocol::SessionItem& item);
  virtual void_t removeItem(uint32_t id);
  virtual void_t updateItem(const BotProtocol::SessionItem& item);

  virtual const HashMap<String, BotProtocol::SessionProperty>& getProperties() const {return properties;}
  virtual const BotProtocol::SessionProperty* getProperty(uint32_t id) const;
  virtual void_t updateProperty(const BotProtocol::SessionProperty& property);
  virtual double getProperty(const String& name, double defaultValue) const;
  virtual String getProperty(const String& name, const String& defaultValue) const;
  virtual void registerProperty(const String& name, double value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String());
  virtual void registerProperty(const String& name, const String& value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String());
  virtual void setProperty(const String& name, double value, uint32_t flags, const String& unit);
  virtual void setProperty(const String& name, const String& value, uint32_t flags, const String& unit);
  virtual void removeProperty(const String& name);

  virtual void_t warning(const String& message);

public: // Broker
  virtual const String& getLastError() const {return error;}
  virtual void_t handleTrade(Bot::Session& session, const DataProtocol::Trade& trade);
};

