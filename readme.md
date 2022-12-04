# Silvanus 
*Let them grow wild*

Silvanus is a simple, RPi-based plant watering and light management system for houseplants. This software is not meant to have every feature needed for a complex plant care schedule, but rather provide as clean and simple of an interface as possible. Management is done over a local network HTTP and has no authentication.

## Hardware Setup

The Raspberry Pi should be configured with a 3 relay hat, with the following setup:

| Relay # | Relay Mode | Control Pin | Hardware |
|-|-|-|-|
1 | NO | 26 | Light
2 | NO | 20 | Water pump
3 | - | 21 | No connection (future expansion)

A capacitive moisture sensor should be wired to the I2C pins and available as device 0x36

## Configuration Parameters

| Parameter Name | Description | Default Value | Unit |
|---------------------|-------------|-------------|-|
| WaterPumpTime     | When watering the plant, how long the water pump runs | 120 | seconds |
| WaterRefractoryPeriod | The minimum amount of time between consecutive waterings | 86400 | seconds |
| WaterTimeTrigger    | The amount of time that must pass since the last watering before another is unconditionally triggered. Must be greater than *WaterRefractoryPeriod* | 345600 | seconds
| WaterMoistureTrigger  | Percentage below which watering is triggered, unless watering already occured more recently than *WaterRefractoryPeriod* | 30 | %
| LightTime          | Relative time after local midnight, when the light should turn on | 25200 (7 AM) | seconds
| LightInterval | Amount of time after turning on that the light should run for | 43200 (12 hours) | seconds