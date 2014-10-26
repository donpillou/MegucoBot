
#pragma once

#include "Tools/Bot.h"
#include "Tools/TradeHandler.h"

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
    double buyInBase;
    double sellInComm;
    timestamp_t lastBuyInTime;
    timestamp_t lastSellInTime;
    timestamp_t lastAssetBuyTime;
    timestamp_t lastAssetSellTime;
    double balanceBase;
    double balanceComm;
    double availableBalanceBase;
    double availableBalanceComm;
    TradeHandler tradeHandler;

    virtual ~Session() {}

    void_t checkBuyIn(const DataProtocol::Trade& trade, const TradeHandler::Values& values);
    void_t checkSellIn(const DataProtocol::Trade& trade, const TradeHandler::Values& values);

    void_t checkAssetBuy(const DataProtocol::Trade& trade);
    void_t checkAssetSell(const DataProtocol::Trade& trade);

    void_t resetBetOrders();
    void_t updateAvailableBalance();
    void_t applyBalanceUpdate(double base, double comm);

    double getBuyInBase(double currentPrice, const TradeHandler::Values& values) const;
    double getSellInComm(double currentPrice, const TradeHandler::Values& values) const;

  private: // Bot::Session
    virtual void_t handleTrade(const DataProtocol::Trade& trade, timestamp_t tradeAge);
    virtual void_t handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleBuyTimeout(uint32_t orderId);
    virtual void_t handleSellTimeout(uint32_t orderId);
    virtual void_t handlePropertyUpdate(const BotProtocol::SessionProperty& property);
    virtual void_t handleAssetUpdate(const BotProtocol::SessionAsset& asset);
    virtual void_t handleAssetRemoval(const BotProtocol::SessionAsset& asset);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
  virtual timestamp_t getMaxTradeAge() const {return TradeHandler::getMaxTradeAge();}
};
