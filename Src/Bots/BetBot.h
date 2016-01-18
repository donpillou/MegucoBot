
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
    uint64_t buyInOrderId;
    uint64_t sellInOrderId;;
    double maxBuyInPrice;
    double minSellInPrice;
    double buyInIncline;
    double sellInIncline;
    double buyInStartPrice;
    double sellInStartPrice;
    double buyInBase;
    double sellInComm;
    int64_t lastBuyInTime;
    int64_t lastSellInTime;
    int64_t lastAssetBuyTime;
    int64_t lastAssetSellTime;
    double balanceBase;
    double balanceComm;
    double availableBalanceBase;
    double availableBalanceComm;
    TradeHandler tradeHandler;

    //virtual ~Session() {}

    void_t checkBuyIn(const meguco_trade_entity& trade, const TradeHandler::Values& values);
    void_t checkSellIn(const meguco_trade_entity& trade, const TradeHandler::Values& values);

    void_t checkAssetBuy(const meguco_trade_entity& trade);
    void_t checkAssetSell(const meguco_trade_entity& trade);

    void_t resetBetOrders();
    void_t updateAvailableBalance();
    void_t applyBalanceUpdate(double base, double comm);

    double getBuyInBase(double currentPrice, const TradeHandler::Values& values) const;
    double getSellInComm(double currentPrice, const TradeHandler::Values& values) const;

  private: // Bot::Session
    virtual void_t handleTrade(const meguco_trade_entity& trade, int64_t tradeAge);
    virtual void_t handleBuy(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction);
    virtual void_t handleSell(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction);
    virtual void_t handleBuyTimeout(uint64_t orderId);
    virtual void_t handleSellTimeout(uint64_t orderId);
    virtual void_t handlePropertyUpdate(const meguco_user_session_property_entity& property);
    virtual void_t handleAssetUpdate(const meguco_user_session_asset_entity& asset);
    virtual void_t handleAssetRemoval(const meguco_user_session_asset_entity& asset);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
  virtual int64_t getMaxTradeAge() const {return TradeHandler::getMaxTradeAge();}
};
