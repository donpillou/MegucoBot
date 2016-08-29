
#pragma once

#include <nstd/HashMap.h>
#include <nstd/Buffer.h>

#include "Tools/Broker.h"

class Main;

class LiveBroker : public Broker
{
public:
  LiveBroker(Main& main, const String& currencyBase, const String& currencyComm);

private:
  Main& main;
  String currencyBase;
  String currencyComm;
  String error;
  HashMap<uint64_t, Bot::Order> openOrders;
  int64_t time;
  int64_t lastBuyTime;
  int64_t lastSellTime;
  int64_t lastOrderRefreshTime;
  HashMap<uint64_t, Bot::Transaction> transactions;
  HashMap<uint64_t, Bot::Asset> assets;
  HashMap<uint64_t, Bot::Property> properties;
  HashMap<String, Bot::Property*> propertiesByName;

private:
  void_t refreshOrders(Bot::Session& botSession);
  void_t cancelTimedOutOrders(Bot::Session& botSession);
  void_t registerProperty(const String& name, const String& value, meguco_user_session_property_type type, uint32_t flags, const String& unit);

private: // Bot::Broker
  virtual const String& getCurrencyBase() const {return currencyBase;};
  virtual const String& getCurrencyComm() const {return currencyComm;};

  virtual int64_t getTime() const {return time;}

  virtual bool_t buy(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount);
  virtual bool_t sell(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount);
  virtual bool_t cancelOder(uint64_t id);
  virtual const HashMap<uint64_t, Bot::Order> getOrders() const {return openOrders;}
  virtual const Bot::Order* getOrder(uint64_t id) const;
  virtual size_t getOpenBuyOrderCount() const;
  virtual size_t getOpenSellOrderCount() const;
  virtual int64_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual int64_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual const HashMap<uint64_t, Bot::Asset>& getAssets() const {return assets;}
  virtual const Bot::Asset* getAsset(uint64_t id) const;
  virtual bool_t createAsset2(Bot::Asset& asset);
  virtual void_t removeAsset(uint64_t id);
  virtual void_t updateAsset2(const Bot::Asset& asset);

  virtual double getProperty(const String& name, double defaultValue) const;
  virtual String getProperty(const String& name, const String& defaultValue) const;
  virtual void_t registerProperty(const String& name, double value, uint32_t flags = meguco_user_session_property_none, const String& unit = String());
  virtual void_t registerProperty(const String& name, const String& value, uint32_t flags = meguco_user_session_property_none, const String& unit = String());
  virtual void_t setProperty(const String& name, double value);
  virtual void_t setProperty(const String& name, const String& value);

  virtual void_t addMarker(meguco_user_session_marker_type markerType);
  virtual void_t warning(const String& message);

public: // Broker
  virtual void_t registerOrder2(const Bot::Order& order);
  virtual void_t registerAsset2(const Bot::Asset& asset);
  virtual void_t unregisterAsset(uint64_t id);
  virtual void_t registerProperty2(const Bot::Property& property);

  virtual const Bot::Property* getProperty(uint64_t id);

  virtual const String& getLastError() const {return error;}
  virtual void_t handleTrade2(Bot::Session& session, const Bot::Trade& trade, bool_t replayed);
};

