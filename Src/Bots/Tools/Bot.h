
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>
#include <nstd/HashMap.h>

#include <megucoprotocol.h>

class Bot
{
public:
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
    virtual const HashMap<uint64_t, meguco_user_broker_order_entity> getOrders() const = 0;
    virtual const meguco_user_broker_order_entity* getOrder(uint64_t id) const = 0;
    virtual size_t getOpenBuyOrderCount() const = 0;
    virtual size_t getOpenSellOrderCount() const = 0;
    virtual int64_t getTimeSinceLastBuy() const = 0;
    virtual int64_t getTimeSinceLastSell() const = 0;

    virtual const HashMap<uint64_t, meguco_user_session_asset_entity>& getAssets() const = 0;
    virtual const meguco_user_session_asset_entity* getAsset(uint64_t id) const = 0;
    virtual bool_t createAsset(meguco_user_session_asset_entity& asset) = 0;
    virtual void_t removeAsset(uint64_t id) = 0;
    virtual void_t updateAsset(const meguco_user_session_asset_entity& asset) = 0;

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
    virtual void_t handleTrade(const meguco_trade_entity& trade, int64_t tradeAge) = 0;
    virtual void_t handleBuy(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction) = 0;
    virtual void_t handleSell(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction) = 0;
    virtual void_t handleBuyTimeout(uint64_t orderId) = 0;
    virtual void_t handleSellTimeout(uint64_t orderId) = 0;
    virtual void_t handlePropertyUpdate(const meguco_user_session_property_entity& property) = 0;
    virtual void_t handleAssetUpdate(const meguco_user_session_asset_entity& asset) = 0;
    virtual void_t handleAssetRemoval(const meguco_user_session_asset_entity& asset) = 0;
  };
  
  virtual ~Bot() {}
  virtual Session* createSession(Broker& broker) = 0;
  virtual int64_t getMaxTradeAge() const = 0;
};
