
# MegucoServer

The MegucoServer project harbours a collection of services that are part of the Meguco Framework. The Meguco Framework in itself is a project intended to keep me entertained. Its goal is to develop and explore a Framework to analyze live trade data from Bitcoin exchanges and to run trading bots. A secondary objective of all this is to create tools like libnstd and ZlimDB to accomplish tasks like this with ease. The target platform of the MegucoServer is a modern low powered ARM Linux environments like the Raspberry Pi. (But it does also run Windows and x86 or x86_64 machines.)

The Meguco Framework consists of the following components:

```
ZlimDB (server sided database and messaging server)
 |- MegucoServer (server sided process manager)
 |   |- User Service (a service to start Brokers and Sessions)
 |   |- Market Service (a service to start Markers)
 |   |- Brokers (exchange account managers)
 |   |- Sessions (trading bots)
 |   |- Markets (live exchange trade data collectors)
 |- MegucoClient (client sided user interface to monitor trade data and control trading bots)
```

