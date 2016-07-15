
#pragma once

#include <nstd/Math.h>

#include "Bot.h"

class TradeHandler
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
      double front;
      double back;
    };
    RegressionLine regressions[(int)numOfRegressions];
    RegressionLine bellRegressions[(int)numOfBellRegressions];
  };

public:
  static int64_t getMaxTradeAge() {return depths[sizeof(depths) / sizeof(*depths) - 1] * 1000ULL;}

public:
  void add(const Bot::Trade& trade, int64_t tradeAge)
  {
    uint64_t tradeAgeSecs = tradeAge / 1000ULL;
    uint64_t timeSecs = trade.time / 1000ULL;

    for(int i = 0; i < (int)numOfRegressions; ++i)
      averager[i].add(timeSecs, tradeAgeSecs, trade.amount, trade.price, depths[i]);

    /*
    for(int i = 0; i < (int)numOfBellRegressions; ++i)
      bellAverager[i].add(timeSecs, tradeAgeSecs, trade.amount, trade.price, depths[i]);
      */
  }

  bool_t isComplete() const {return averager[(int)numOfRegressions - 1].isComplete();}

  Values& getValues()
  {
    for(int i = 0; i < (int)numOfRegressions; ++i)
      averager[i].getLine(values.regressions[i]);
    /*
    for(int i = 0; i < (int)numOfBellRegressions; ++i)
      bellAverager[i].getLine(depths[i], values.bellRegressions[i]);
      */
    return values;
  }

private:
  class Averager
  {
  public:
    Averager() : complete(false), startTime(0), x(0), sumXY(0.), sumY(0.), sumX(0.), sumXX(0.), sumN(0.), newSumXY(0.), newSumY(0.), newSumX(0.), newSumXX(0.), newSumN(0.), minPrice(1.7976931348623158e+308), maxPrice(0.), needMinMaxUpdate(false) {}

    bool_t isComplete() const {return complete;}

    void add(uint64_t time, uint64_t tradeAge, double amount, double price, uint64_t maxAge)
    {
      if(tradeAge > maxAge)
        return;

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

      limitToAge(maxAge);
    }

    void getLine(Values::RegressionLine& rl)
    {
      rl.incline = (sumN * sumXY - sumX * sumY) / (sumN * sumXX - sumX * sumX);
      double ar = (sumXX * sumY - sumX * sumXY) / (sumN * sumXX - sumX * sumX);
      rl.price = ar + rl.incline * x;
      rl.average = sumY / sumN;
      if(needMinMaxUpdate)
        updateMinMax();
      rl.min = minPrice;
      rl.max = maxPrice;
      rl.back = data.back().y;
      rl.front = data.front().y;
    }

  private:
    struct DataEntry
    {
      uint64_t time;
      double x;
      double n;
      double y;
    };

  private:
    bool complete;
    List<DataEntry> data;
    uint64_t startTime;
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

  private:
    void limitToAge(uint64_t maxAge)
    {
      if(data.isEmpty())
        return;
      uint64_t now = data.back().time;

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
        complete = true;
        if(newCount == (unsigned int)data.size())
          useNewSum();
      }
    }

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
  /*
  class BellAverager
  {
  public:
    BellAverager() : startTime(0), x(0), sumXY(0.), sumY(0.), sumX(0.), sumXX(0.), sumN(0.), minPrice(1.7976931348623158e+308), maxPrice(0.), needMinMaxUpdate(false) {}

    void add(uint64_t time, uint64_t tradeAge, double amount, double price, uint64_t ageDeviation)
    {
      uint64_t ageDeviationTimes3 = ageDeviation * 3;
      if(tradeAge > ageDeviationTimes3)
        return;

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

    void getLine(int64_t ageDeviation, Values::RegressionLine& rl)
    {
      // recompute
      sumXY = sumY = sumX = sumXX = sumN = 0.;
      double deviation = (double)ageDeviation;
      int64_t endTime = data.back().time;
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
      uint64_t time;
      double x;
      double n;
      double y;
    };
    List<DataEntry> data;
    uint64_t startTime;
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
  */
private:
  static const uint64_t depths[13];

private:
  Values values;
  Averager averager[(int)numOfRegressions];
  //BellAverager bellAverager[(int)numOfBellRegressions];
};
