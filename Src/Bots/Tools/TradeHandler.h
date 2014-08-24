
#pragma once

#include <nstd/Math.h>

#include "Bot.h"
#include "DataProtocol.h"

class TradeHandler
{
public:

  Bot::Values values;

  void add(const DataProtocol::Trade& trade, timestamp_t tradeAge)
  {
    static const uint64_t depths[] = {1 * 60, 3 * 60, 5 * 60, 10 * 60, 15 * 60, 20 * 60, 30 * 60, 1 * 60 * 60, 2 * 60 * 60, 4 * 60 * 60, 6 * 60 * 60, 12 * 60 * 60, 24 * 60 * 60};

    uint64_t tradeAgeSecs = tradeAge / 1000ULL;
    if(tradeAgeSecs > depths[sizeof(depths) / sizeof(*depths) - 1] * 3ULL)
      return;

    bool updateValues = tradeAge == 0;
    uint64_t time = trade.time / 1000ULL;

    for(int i = 0; i < (int)Bot::numOfRegressions; ++i)
    {
      if(tradeAgeSecs <= depths[i])
      {
        averager[i].add(time, trade.amount, trade.price);
        averager[i].limitToAge(depths[i]);
        if(updateValues)
          averager[i].getLine(values.regressions[i]);
      }
    }

    for(int i = 0; i < (int)Bot::numOfBellRegressions; ++i)
    {
      if(tradeAgeSecs <= depths[i] * 3ULL)
      {
        bellAverager[i].add(time, trade.amount, trade.price, depths[i]);
        if(updateValues)
          bellAverager[i].getLine(depths[i], values.bellRegressions[i]);
      }
    }
  }

private:
  class Averager
  {
  public:
    Averager() : startTime(0), x(0), sumXY(0.), sumY(0.), sumX(0.), sumXX(0.), sumN(0.), newSumXY(0.), newSumY(0.), newSumX(0.), newSumXX(0.), newSumN(0.), minPrice(1.7976931348623158e+308), maxPrice(0.), needMinMaxUpdate(false) {}

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

      if(price > maxPrice)
        maxPrice = price;
      if(price < minPrice)
        minPrice = price;
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

        if(y >= maxPrice || y <= minPrice)
          needMinMaxUpdate = true;

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

        if(y >= maxPrice || y <= minPrice)
          needMinMaxUpdate = true;

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

    void getLine(Bot::Values::RegressionLine& rl)
    {
      rl.incline = (sumN * sumXY - sumX * sumY) / (sumN * sumXX - sumX * sumX);
      double ar = (sumXX * sumY - sumX * sumXY) / (sumN * sumXX - sumX * sumX);
      rl.price = ar + rl.incline * x;
      rl.average = sumY / sumN;
      if(needMinMaxUpdate)
        updateMinMax();
      rl.min = minPrice;
      rl.max = maxPrice;
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

    double minPrice;
    double maxPrice;
    bool needMinMaxUpdate;

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

    void updateMinMax()
    {
      minPrice = 1.7976931348623158e+308;
      maxPrice = 0.;
      double price;
      for(List<DataEntry>::Iterator i = data.begin(), end = data.end(); i != end; ++i)
      {
        price = i->y;
        if(price > maxPrice)
          maxPrice = price;
        if(price < minPrice)
          minPrice = price;
      }
      needMinMaxUpdate = false;
    }
  };

  class BellAverager
  {
  public:
    BellAverager() : startTime(0), x(0), sumXY(0.), sumY(0.), sumX(0.), sumXX(0.), sumN(0.), minPrice(1.7976931348623158e+308), maxPrice(0.), needMinMaxUpdate(false) {}

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

      if(price > maxPrice)
        maxPrice = price;
      if(price < minPrice)
        minPrice = price;

      timestamp_t ageDeviationTimes3 = ageDeviation * 3;
      for(;;)
      {
        DataEntry& entry = data.front();
        if(time - entry.time > ageDeviationTimes3)
        {
          if(entry.y >= maxPrice || entry.y <= minPrice)
            needMinMaxUpdate = true;
          data.removeFront();
        }
        else
          break;
      }
    }

    void getLine(timestamp_t ageDeviation, Bot::Values::RegressionLine& rl)
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

      rl.incline = (sumN * sumXY - sumX * sumY) / (sumN * sumXX - sumX * sumX);
      double ar = (sumXX * sumY - sumX * sumXY) / (sumN * sumXX - sumX * sumX);
      rl.price = ar + rl.incline * x;
      rl.average = sumY / sumN;

      if(needMinMaxUpdate)
        updateMinMax();

      rl.min = minPrice;
      rl.max = maxPrice;
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

    void updateMinMax()
    {
      minPrice = 1.7976931348623158e+308;
      maxPrice = 0.;
      double price;
      for(List<DataEntry>::Iterator i = data.begin(), end = data.end(); i != end; ++i)
      {
        price = i->y;
        if(price > maxPrice)
          maxPrice = price;
        if(price < minPrice)
          minPrice = price;
      }
      needMinMaxUpdate = false;
    }

    double minPrice;
    double maxPrice;
    bool needMinMaxUpdate;
  };

  Averager averager[(int)Bot::numOfRegressions];
  BellAverager bellAverager[(int)Bot::numOfBellRegressions];
};
