# BQ25672
Arduino library for the BQ25672 from Texas instruments.

## Description
The BQ25672 is a 1-4S Li-ion battery charger IC including many adjustable options and reading functions. For full details check out the datasheet at: https://www.ti.com/product/BQ25672 

## Compatibility
| Microcontroller   | Tested<br/>works | Does not<br/>work |
|:------------------|:----------------:|:-----------------:|
| ESP32             |        X         |                   |


### Things to beware of...
The BQ25672 has a watchdog timer enabled by default. Changed settings are reset after the watchdog timer has passed. The timer can be disabled with:

```cpp
BQ25672.setWatchdogTimerTime(0);  //Replace 'BQ25672' with the class name you defined
```

When writing a value that is out of range (which is currently allowed by the library), the charger discards the command. When the value is not a multiple of the LSB, the value is rounded down to the closed valid value by the library.

## TODO
- Implement Masks.
- Implement boundaries for write functions (check whether written value is within valid range).
- Maybe read back register after being written to check if the write action was successful.
- Miss anything? Let me know in the Issue section :)