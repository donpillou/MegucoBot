
#pragma once

#include "Tools/Bot.h"

class BetBot : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    Session(Broker& broker);

  private:
    Broker& broker;
    uint32_t buyInOrderId;
    uint32_t sellInOrderId;;
    double maxBuyInPrice;
    double minSellInPrice;
    double buyInIncline;
    double sellInIncline;
    double buyInStartPrice;
    double sellInStartPrice;
    timestamp_t lastBuyInTime;
    timestamp_t lastSellInTime;
    double balanceBase;
    double balanceComm;
    double availableBalanceBase;
    double availableBalanceComm;

    virtual ~Session() {}

    void_t checkBuyIn(const DataProtocol::Trade& trade, const Values& values);
    void_t checkSellIn(const DataProtocol::Trade& trade, const Values& values);

    void_t checkAssetBuy(const DataProtocol::Trade& trade);
    void_t checkAssetSell(const DataProtocol::Trade& trade);

    void_t resetBetOrders();
    void_t updateAvailableBalance();
    void_t applyBalanceUpdate(double base, double comm);

    double getBuyInBase(double currentPrice) const;
    double getSellInComm(double currentPrice) const;

  private: // Bot::Session
    virtual void_t handle(const DataProtocol::Trade& trade, const Values& values);
    virtual void_t handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleBuyTimeout(uint32_t orderId);
    virtual void_t handleSellTimeout(uint32_t orderId);
    virtual void_t handlePropertyUpdate(BotProtocol::SessionProperty& property);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
};
