# How to build
Specify target device (AI_MODULE or GRC_DEVBOARD) in BUILD_TARGET variable:
```bash
idf.py -DBUILD_TARGET=<target device> build
idf.py -p <port> flash monitor
```
