
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>
#include <nstd/HashMap.h>

#include "DataProtocol.h"
#include "BotProtocol.h"

class Bot
{
public:
  enum Regressions
  {
    regression1m,
    regression3m,
    regression5m,
    regression10m,
    regression15m,
    regression20m,
    regression30m,
    regression1h,
    regression2h,
    regression4h,
    regression6h,
    regression12h,
    regression24h,
    numOfRegressions,
  };

  enum BellRegressions
  {
    bellRegression1m,
    bellRegression3m,
    bellRegression5m,
    bellRegression10m,
    bellRegression15m,
    bellRegression20m,
    bellRegression30m,
    bellRegression1h,
    bellRegression2h,
    numOfBellRegressions,
  };

  struct Values
  {
    struct RegressionLine
    {
      double price; // a
      double incline; // b
      double average;
      double min;
      double max;
    };
    RegressionLine regressions[(int)numOfRegressions];
    RegressionLine bellRegressions[(int)numOfBellRegressions];
  };

  class Broker
  {
  public:
    virtual ~Broker() {}

    virtual const String& getCurrencyBase() const = 0;
    virtual const String& getCurrencyComm() const = 0;

    virtual bool_t buy(double price, double amount, double total, timestamp_t timeout, uint32_t* id = 0, double* orderedAmount = 0) = 0;
    virtual bool_t sell(double price, double amount, double total, timestamp_t timeout, uint32_t* id = 0, double* orderedAmount = 0) = 0;
    virtual bool_t cancelOder(uint32_t id) = 0;
    virtual const HashMap<uint32_t, BotProtocol::Order> getOrders() const = 0;
    virtual size_t getOpenBuyOrderCount() const = 0;
    virtual size_t getOpenSellOrderCount() const = 0;
    virtual timestamp_t getTimeSinceLastBuy() const = 0;
    virtual timestamp_t getTimeSinceLastSell() const = 0;

    virtual const HashMap<uint32_t, BotProtocol::SessionAsset>& getAssets() const = 0;
    virtual const BotProtocol::SessionAsset* getAsset(uint32_t id) const = 0;
    virtual bool_t createAsset(BotProtocol::SessionAsset& asset) = 0;
    virtual void_t removeAsset(uint32_t id) = 0;
    virtual void_t updateAsset(const BotProtocol::SessionAsset& asset) = 0;

    virtual const HashMap<String, BotProtocol::SessionProperty>& getProperties() const = 0;
    virtual const BotProtocol::SessionProperty* getProperty(uint32_t id) const = 0;
    virtual void_t updateProperty(const BotProtocol::SessionProperty& property) = 0;
    virtual double getProperty(const String& name, double defaultValue) const = 0;
    virtual String getProperty(const String& name, const String& defaultValue) const = 0;
    virtual void_t registerProperty(const String& name, double value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String()) = 0;
    virtual void_t registerProperty(const String& name, const String& value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String()) = 0;
    virtual void_t setProperty(const String& name, double value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String()) = 0;
    virtual void_t setProperty(const String& name, const String& value, uint32_t flags = BotProtocol::SessionProperty::none, const String& unit = String()) = 0;
    virtual void_t removeProperty(const String& name) = 0;

    virtual void_t addMarker(BotProtocol::Marker::Type markerType) = 0;
    virtual void_t warning(const String& message) = 0;
  };

  class Session
  {
  public:
    virtual ~Session() {};
    virtual void_t handleTrade(const DataProtocol::Trade& trade, const Values& values) = 0;
    virtual void_t handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction) = 0;
    virtual void_t handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction) = 0;
    virtual void_t handleBuyTimeout(uint32_t orderId) = 0;
    virtual void_t handleSellTimeout(uint32_t orderId) = 0;
    virtual void_t handlePropertyUpdate(const BotProtocol::SessionProperty& property) = 0;
    virtual void_t handleAssetUpdate(const BotProtocol::SessionAsset& asset) = 0;
    virtual void_t handleAssetRemoval(const BotProtocol::SessionAsset& asset) = 0;
  };
  
  virtual ~Bot() {}
  virtual Session* createSession(Broker& broker) = 0;
};
