
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>
#include <nstd/HashMap.h>

#include <megucoprotocol.h>

class Bot
{
public:
  class Order
  {
  public:
    uint64_t id;
    uint64_t time;
    double price;
    double amount;
    double total;
    uint64_t raw_id;
    uint64_t timeout;
    uint8_t type;
    uint8_t state;

  public:
    Order() : id(0), time(0) {}
    Order(const meguco_user_broker_order_entity& entity);
    operator meguco_user_broker_order_entity() const;
  };

  class Asset
  {
  public:
    uint64_t id;
    uint64_t time;
    double price;
    double invest_comm;
    double invest_base;
    double balance_comm;
    double balance_base;
    double profitable_price;
    double flip_price;
    uint64_t order_id;
    int64_t last_transaction_time;
    uint8_t type;
    uint8_t state;

  public:
    Asset() : id(0), time(0) {}
    Asset(const meguco_user_session_asset_entity& entity);
    operator meguco_user_session_asset_entity() const;
  };

  class Property
  {
  public:
    uint64_t id;
    uint64_t time;
    uint32_t flags;
    uint8_t type;
    String name;
    String value;
    String unit;

  public:
    Property() : id(0), time(0) {}
    Property(const meguco_user_session_property_entity& entity, const String& name, const String& value, const String& unit);
    operator meguco_user_session_property_entity() const;
  };

  class Transaction
  {
  public:
    uint64_t id;
    uint64_t time;
    double price;
    double amount;
    double total;
    uint64_t raw_id;
    uint8_t type;

  public:
    Transaction() : id(0), time(0) {}
    Transaction(const meguco_user_broker_transaction_entity& entity);
    operator meguco_user_broker_transaction_entity() const;
  };

  class Trade
  {
  public:
    uint64_t id;
    uint64_t time;
    double price;
    double amount;
    uint32_t flags;

  public:
    Trade(const meguco_trade_entity& entity);
  };

  class Marker
  {
  public:
    uint64_t id;
    uint64_t time;
    uint8_t type;

  public:
    Marker() : id(0), time(0) {}
    //Marker(const meguco_user_session_marker_entity& entity);
    operator meguco_user_session_marker_entity() const;
  };

  class Broker
  {
  public:
    virtual ~Broker() {}

    virtual int64_t getTime() const = 0;

    virtual const String& getCurrencyBase() const = 0;
    virtual const String& getCurrencyComm() const = 0;

    virtual bool_t buy(double price, double amount, double total, int64_t timeout, uint64_t* id = 0, double* orderedAmount = 0) = 0;
    virtual bool_t sell(double price, double amount, double total, int64_t timeout, uint64_t* id = 0, double* orderedAmount = 0) = 0;
    virtual bool_t cancelOder(uint64_t id) = 0;
    virtual const HashMap<uint64_t, Order> getOrders() const = 0;
    virtual const Order* getOrder(uint64_t id) const = 0;
    virtual size_t getOpenBuyOrderCount() const = 0;
    virtual size_t getOpenSellOrderCount() const = 0;
    virtual int64_t getTimeSinceLastBuy() const = 0;
    virtual int64_t getTimeSinceLastSell() const = 0;

    virtual const HashMap<uint64_t, Asset>& getAssets() const = 0;
    virtual const Asset* getAsset(uint64_t id) const = 0;
    virtual bool_t createAsset(Asset& asset) = 0;
    virtual void_t removeAsset(uint64_t id) = 0;
    virtual void_t updateAsset(const Asset& asset) = 0;

    virtual double getProperty(const String& name, double defaultValue) const = 0;
    virtual String getProperty(const String& name, const String& defaultValue) const = 0;
    virtual void_t registerProperty(const String& name, double value, uint32_t flags = meguco_user_session_property_none, const String& unit = String()) = 0;
    virtual void_t registerProperty(const String& name, const String& value, uint32_t flags = meguco_user_session_property_none, const String& unit = String()) = 0;
    virtual void_t setProperty(const String& name, double value) = 0;
    virtual void_t setProperty(const String& name, const String& value) = 0;

    virtual void_t addMarker(meguco_user_session_marker_type markerType) = 0;
    virtual void_t warning(const String& message) = 0;
  };

  class Session
  {
  public:
    virtual ~Session() {};
    virtual void_t handleTrade(const Trade& trade, int64_t tradeAge) = 0;
    virtual void_t handleBuy(uint64_t orderId, const Transaction& transaction) = 0;
    virtual void_t handleSell(uint64_t orderId, const Transaction& transaction) = 0;
    virtual void_t handleBuyTimeout(uint64_t orderId) = 0;
    virtual void_t handleSellTimeout(uint64_t orderId) = 0;
    virtual void_t handlePropertyUpdate(const Property& property) = 0;
    virtual void_t handleAssetUpdate(const Asset& asset) = 0;
    virtual void_t handleAssetRemoval(const Asset& asset) = 0;
  };
  
  virtual ~Bot() {}
  virtual Session* createSession(Broker& broker) = 0;
  virtual int64_t getMaxTradeAge() const = 0;
};
