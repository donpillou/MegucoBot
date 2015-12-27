
#pragma once

#include <nstd/HashMap.h>
#include <nstd/Buffer.h>

#include "Tools/Broker.h"

class Main;

class SimBroker : public Broker
{
public:
  SimBroker(Main& main, const String& currencyBase, const String& currencyComm, double tradeFree, int64_t maxTradeAge);

private:
  struct Property
  {
    meguco_user_session_property_entity property;
    String name;
    String value;
    String unit;
  };

private:
  Main& main;
  String currencyBase;
  String currencyComm;
  String error;
  HashMap<uint64_t, meguco_user_broker_order_entity> openOrders;
  int64_t time;
  int64_t lastBuyTime;
  int64_t lastSellTime;
  double tradeFee;
  HashMap<uint64_t, meguco_user_broker_transaction_entity> transactions;
  HashMap<uint64_t, meguco_user_session_asset_entity> assets;
  HashMap<uint64_t, Property> properties;
  HashMap<String, Property*> propertiesByName;
  int64_t startTime;
  int64_t maxTradeAge;

private:
  void_t setProperty(const String& name, const String& value, meguco_user_session_property_type type, uint32_t flags, const String& unit);

private: // Bot::Broker
  virtual const String& getCurrencyBase() const {return currencyBase;};
  virtual const String& getCurrencyComm() const {return currencyComm;};

  virtual bool_t buy(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount);
  virtual bool_t sell(double price, double amount, double total, int64_t timeout, uint64_t* id, double* orderedAmount);
  virtual bool_t cancelOder(uint64_t id);
  virtual const HashMap<uint64_t, meguco_user_broker_order_entity> getOrders() const {return openOrders;}
  virtual const meguco_user_broker_order_entity* getOrder(uint64_t id) const;
  virtual size_t getOpenBuyOrderCount() const;
  virtual size_t getOpenSellOrderCount() const;
  virtual int64_t getTimeSinceLastBuy() const{return time - lastBuyTime;}
  virtual int64_t getTimeSinceLastSell() const {return time - lastSellTime;}

  virtual const HashMap<uint64_t, meguco_user_session_asset_entity>& getAssets() const {return assets;}
  virtual const meguco_user_session_asset_entity* getAsset(uint64_t id) const;
  virtual bool_t createAsset(meguco_user_session_asset_entity& asset);
  virtual void_t removeAsset(uint64_t id);
  virtual void_t updateAsset(const meguco_user_session_asset_entity& asset);

  //virtual const HashMap<String, SessionProperty>& getProperties() const {return properties;}
  //virtual const SessionProperty* getProperty(uint64_t id) const;
  //virtual void_t updateProperty(const SessionProperty& property);
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
  virtual void_t registerTransaction(const meguco_user_broker_transaction_entity& transaction);
  virtual void_t registerOrder(const meguco_user_broker_order_entity& order);
  virtual void_t registerAsset(const meguco_user_session_asset_entity& asset);
  virtual void_t unregisterAsset(uint64_t id);
  virtual void_t registerProperty(const meguco_user_session_property_entity& property, const String& name, const String& value, const String& unit);

  virtual const meguco_user_session_property_entity* getProperty(uint64_t id, String& name, String& value, String& unit);

  virtual const String& getLastError() const {return error;}
  virtual void_t handleTrade(Bot::Session& session, const meguco_trade_entity& trade, bool_t replayed);
};

