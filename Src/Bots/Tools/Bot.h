
#pragma once

#include "Tools/Math.h"

class Bot
{
public:
  enum class Regressions
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

  enum class BellRegressions
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
    };
    RegressionLine regressions[(int)Regressions::numOfRegressions];
    RegressionLine bellRegressions[(int)BellRegressions::numOfBellRegressions];
  };

  class Broker
  {
  public:
    class Transaction
    {
    public:
      enum class Type
      {
        buy,
        sell,
      };

      uint64_t id;
      uint64_t date;
      double price;
      double amount;
      double fee;
      Type type;
    };

    virtual ~Broker() {}
    virtual bool buy(double price, double amount, timestamp_t timeout) = 0;
    virtual bool sell(double price, double amount, timestamp_t timeout) = 0;
    virtual double getBalanceBase() const = 0;
    virtual double getBalanceComm() const = 0;
    virtual double getFee() const = 0;
    virtual unsigned int getOpenBuyOrderCount() const = 0;
    virtual unsigned int getOpenSellOrderCount() const = 0;
    virtual uint64_t getTimeSinceLastBuy() const = 0;
    virtual uint64_t getTimeSinceLastSell() const = 0;

    virtual void getTransactions(List<Transaction>& transactions) const = 0;
    virtual void getBuyTransactions(List<Transaction>& transactions) const = 0;
    virtual void getSellTransactions(List<Transaction>& transactions) const = 0;
    virtual void removeTransaction(uint64_t id) = 0;
    virtual void updateTransaction(uint64_t id, const Transaction& transaction) = 0;

    virtual void warning(const String& message) = 0;
  };

  class Session
  {
  public:
    virtual ~Session() {};
    virtual void setParameters(double* parameters) = 0;
    virtual void handle(const DataProtocol::Trade& trade, const Values& values) = 0;
    virtual void handleBuy(const Broker::Transaction& transaction) = 0;
    virtual void handleSell(const Broker::Transaction& transaction) = 0;
  };

  virtual ~Bot() {}
  virtual Session* createSession(Broker& broker) = 0;
  virtual unsigned int getParameterCount() const = 0;
};

class TradeHandler
{
public:

  Bot::Values values;

  void add(const DataProtocol::Trade& trade, timestamp_t tradeAge)
  {
    uint64_t tradeAgeSecs = tradeAge / 1000ULL;
    bool updateValues = tradeAge == 0;
    uint64_t time = trade.time / 1000ULL;
    uint64_t depths[] = {1 * 60, 3 * 60, 5 * 60, 10 * 60, 15 * 60, 20 * 60, 30 * 60, 1 * 60 * 60, 2 * 60 * 60, 4 * 60 * 60, 6 * 60 * 60, 12 * 60 * 60, 24 * 60 * 60};
    for(int i = 0; i < (int)Bot::Regressions::numOfRegressions; ++i)
    {
      if(tradeAgeSecs <= depths[i])
      {
        averager[i].add(time, trade.amount, trade.price);
        averager[i].limitToAge(depths[i]);
        if(updateValues)
        {
          Bot::Values::RegressionLine& rl = values.regressions[i];
          averager[i].getLine(rl.price, rl.incline, rl.average);
        }
      }
    }

    for(int i = 0; i < (int)Bot::BellRegressions::numOfBellRegressions; ++i)
    {
      if(tradeAgeSecs <= depths[i] * 3ULL)
      {
        bellAverager[i].add(time, trade.amount, trade.price, depths[i]);
        if(updateValues)
        {
          Bot::Values::RegressionLine& rl = values.bellRegressions[i];
          bellAverager[i].getLine(depths[i], rl.price, rl.incline, rl.average);
        }
      }
    }
  }

private:
  class Averager
  {
  public:
    Averager() : startTime(0), x(0), sumXY(0.), sumY(0.), sumX(0.), sumXX(0.), sumN(0.), newSumXY(0.), newSumY(0.), newSumX(0.), newSumXX(0.), newSumN(0.) {}

    void add(timestamp_t time, double amount, double price)
    {
      if(startTime == 0)
        startTime = time;

      x = (double)(time - startTime);
      const double& y = price;
      const double& n = amount;

      const double nx = n * x;
      const double ny = n * y;
      const double nxy = nx * y;
      const double nxx = nx * x;

      sumXY += nxy;
      sumY += ny;
      sumX += nx;
      sumXX += nxx;
      sumN += n;

      newSumXY += nxy;
      newSumY += ny;
      newSumX += nx;
      newSumXX += nxx;
      newSumN += n;
      if(++newCount == (unsigned int)data.size())
        useNewSum();

      DataEntry& dataEntry = data.append(DataEntry());
      dataEntry.time = time;
      dataEntry.x = x;
      dataEntry.y = y;
      dataEntry.n = n;
    }

    void limitToVolume(double amount)
    {
      double totalNToRemove = sumN - amount;
      while(totalNToRemove > 0. && !data.isEmpty())
      {
        DataEntry& dataEntry = data.front();
        double nToRemove= Math::min(dataEntry.n, totalNToRemove);

        const double& x = dataEntry.x;
        const double& y = dataEntry.y;
        const double& n = nToRemove;

        const double nx = n * x;
        const double ny = n * y;
        const double nxy = nx * y;
        const double nxx = nx * x;

        sumXY -= nxy;
        sumY -= ny;
        sumX -= nx;
        sumXX -= nxx;
        sumN -= n;

        if(nToRemove >= dataEntry.n)
        {
          data.removeFront();
          if(newCount == (unsigned int)data.size())
            useNewSum();
        }
        else
          dataEntry.n -= nToRemove;
       
        totalNToRemove -= nToRemove;
      }
    }

    void limitToAge(timestamp_t maxAge)
    {
      if(data.isEmpty())
        return;
      timestamp_t now = data.back().time;

      while(!data.isEmpty())
      {
        DataEntry& dataEntry = data.front();
        if(now - dataEntry.time <= maxAge)
          return;

        const double& x = dataEntry.x;
        const double& y = dataEntry.y;
        const double& n = dataEntry.n;

        const double nx = n * x;
        const double ny = n * y;
        const double nxy = nx * y;
        const double nxx = nx * x;

        sumXY -= nxy;
        sumY -= ny;
        sumX -= nx;
        sumXX -= nxx;
        sumN -= n;

        data.removeFront();
        if(newCount == (unsigned int)data.size())
          useNewSum();
      }
    }

    void getLine(double& a, double& b, double& avg) const
    {
      b = (sumN * sumXY - sumX * sumY) / (sumN * sumXX - sumX * sumX);
      double ar = (sumXX * sumY - sumX * sumXY) / (sumN * sumXX - sumX * sumX);
      a = ar + b * x;
      avg = sumY / sumN;
    }

  private:
    struct DataEntry
    {
      timestamp_t time;
      double x;
      double n;
      double y;
    };
    List<DataEntry> data;
    timestamp_t startTime;
    double x;
    double sumXY;
    double sumY;
    double sumX;
    double sumXX;
    double sumN;
    double newSumXY;
    double newSumY;
    double newSumX;
    double newSumXX;
    double newSumN;
    unsigned newCount;

    void useNewSum()
    {
      sumXY = newSumXY;
      sumY = newSumY;
      sumX = newSumX;
      sumXX = newSumXX;
      sumN = newSumN;

      sumXY = 0;
      sumY = 0;
      sumX = 0;
      sumXX = 0;
      sumN = 0;
      newCount = 0;
    }
  };

  class BellAverager
  {
  public:
    BellAverager() : startTime(0), x(0), sumXY(0.), sumY(0.), sumX(0.), sumXX(0.), sumN(0.) {}

    void add(timestamp_t time, double amount, double price, timestamp_t ageDeviation)
    {
      if(startTime == 0)
        startTime = time;

      x = (double)(time - startTime);
      const double& y = price;
      const double& n = amount;

      DataEntry& dataEntry = data.append(DataEntry());
      dataEntry.time = time;
      dataEntry.x = x;
      dataEntry.y = y;
      dataEntry.n = n;

      timestamp_t ageDeviationTimes3 = ageDeviation * 3;
      while(time - data.front().time > ageDeviationTimes3)
        data.removeFront();
    }

    void getLine(timestamp_t ageDeviation, double& a, double& b, double& avg)
    {
      // recompute
      sumXY = sumY = sumX = sumXX = sumN = 0.;
      double deviation = (double)ageDeviation;
      timestamp_t endTime = data.back().time;
      for(List<DataEntry>::Iterator i = --List<DataEntry>::Iterator(data.end()), begin = data.begin();; --i)
      {
        DataEntry& dataEntry = *i;
        double dataAge = (double)(endTime - dataEntry.time);
        double dataWeight = Math::exp(-0.5 * (dataAge * dataAge) / (deviation * deviation));

        if(dataWeight < 0.01)
        {
          for(List<DataEntry>::Iterator j = data.begin(); j != i; ++j)
          {
            ++j;
            data.removeFront();
          }
          data.removeFront();
          break;
        }

        const double n = dataEntry.n * dataWeight;
        const double nx = n * dataEntry.x;
        sumXY += nx * dataEntry.y;
        sumY += n * dataEntry.y;
        sumX += nx;
        sumXX += nx * dataEntry.x;
        sumN += n;

        if(i == begin)
          break;
      }

      b = (sumN * sumXY - sumX * sumY) / (sumN * sumXX - sumX * sumX);
      double ar = (sumXX * sumY - sumX * sumXY) / (sumN * sumXX - sumX * sumX);
      a = ar + b * x;
      avg = sumY / sumN;
    }

  private:
    struct DataEntry
    {
      timestamp_t time;
      double x;
      double n;
      double y;
    };
    List<DataEntry> data;
    timestamp_t startTime;
    double x;
    double sumXY;
    double sumY;
    double sumX;
    double sumXX;
    double sumN;
  };

  Averager averager[(int)Bot::Regressions::numOfRegressions];
  BellAverager bellAverager[(int)Bot::BellRegressions::numOfBellRegressions];
};
