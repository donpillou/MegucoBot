
#pragma once

#include "Tools/Bot.h"
#include "Tools/TradeHandler.h"

class BetBot2 : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    Session(Broker& broker);

  private:
    struct ValueSample
    {
      int64_t time;
      double value;
    };

  private:
    Broker& broker;
    uint64_t buyInOrderId;
    uint64_t sellInOrderId;;
    double buyInPrice;
    double sellInPrice;
    int64_t lastBuyInTime;
    int64_t lastSellInTime;
    int64_t lastAssetBuyTime;
    int64_t lastAssetSellTime;
    double balanceBase;
    double balanceComm;
    double availableBalanceBase;
    double availableBalanceComm;
    TradeHandler tradeHandler;
    //List<ValueSample> ranges;
    List<ValueSample> inclines;

    enum State
    {
      idle,
      waitForDecrease,
      waitForTrade,
    };

    State buyInState;
    State sellInState;

    //virtual ~Session() {}

    void_t checkBuyIn(const Bot::Trade& trade, const TradeHandler::Values& values);
    void_t checkSellIn(const Bot::Trade& trade, const TradeHandler::Values& values);

    void_t checkAssetBuy(const Bot::Trade& trade);
    void_t checkAssetSell(const Bot::Trade& trade);

    void_t resetBetOrders();
    void_t updateAvailableBalance();
    void_t applyBalanceUpdate(double base, double comm);

    double getBuyInBase(double currentPrice, const TradeHandler::Values& values) const;
    double getSellInComm(double currentPrice, const TradeHandler::Values& values) const;

  private: // Bot::Session
    virtual void_t handleTrade(const Trade& trade, int64_t tradeAge);
    virtual void_t handleBuy(uint64_t orderId, const Transaction& transaction);
    virtual void_t handleSell(uint64_t orderId, const Transaction& transaction);
    virtual void_t handleBuyTimeout(uint64_t orderId);
    virtual void_t handleSellTimeout(uint64_t orderId);
    virtual void_t handlePropertyUpdate(const Property& property);
    virtual void_t handleAssetUpdate(const Asset& asset);
    virtual void_t handleAssetRemoval(const Asset& asset);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
  virtual int64_t getMaxTradeAge() const {return TradeHandler::getMaxTradeAge();}
};
