# Silvanus 
*Let them grow wild*

Silvanus is a simple, RPi-based plant watering and light management system for houseplants. This software is not meant to have every feature needed for a complex plant care schedule, but rather provide as clean and simple framework for future customization. Management is done over a local network HTTP and has no authentication.

## Specialty Hardware Required
- [Electronics-Salon RPi 3x Power Relay Hat](https://www.amazon.com/dp/B07CZL2SKN)
- [Gikfun 12V DC Peristaltic Dosing Pump AE1207](https://www.amazon.com/dp/B01IUVHB8E)
- [Adafruit STEMMA Soil Sensor](https://www.adafruit.com/product/4026)
- [2mm ID x 4mm OD Silicone Tubing](https://www.amazon.com/gp/product/B08JKK8BCM/r)
- 5v power supply for RPi
- 12v power supply for pump

The Raspberry Pi relay hat is rated to control AC power up to 250V / 10A or DC power up to 30V / 5A. This means you can turn on and off grow lights by running the the hot lead of their hot AC line straight through the a relay. The same goes for the water pump. Simply run one of its power lines through a relay to control it.

The rest of the hardware is at your discretion how it's wired, what housing to use, etc. My reference setup uses a large junction box.

## Hardware Setup

The Raspberry Pi should be configured with a 3 relay hat, with the following setup:

| Relay # | Relay Mode | Control Pin | Hardware |
|-|-|-|-|
1 | NO | 26 | Light power (hot)
2 | NO | 20 | Pump power (+)
3 | - | 21 | No connection (future expansion)

A capacitive moisture sensor should be wired to the I2C pins and available as device 0x36

## Usage
Once the software is loaded up on to a pi and the hardware is all configured, start Silvanus (there is a systemd config included for unattended startup) and access http://<__ip or hostname of pi__> to view the web GUI and configuration interface.

Settings are saved in "/boot/SilvanusConfig.json". This is conveniently located in the FAT partiiton of the RPi SD card. Some additional settings like the port of the HTTP service can be edited in the json file after first launch.

## Configuration Parameters

| Parameter Name | Description | Default Value | Unit |
|---------------------|-------------|-------------|-|
| waterTime | Plants are watered daily. This setting determines when they are watered | 25200<br /> *(7 AM)* | seconds after midnight |
| waterAmountPerDay | How much water is depensed per day. | 100 | mL |
| waterFlowRate | How quickly the pump dispenses water. The default is based on my pump. Change this value if it seems to be significantly under- or over-watering. | 1.3 | mL / sec |
| lightTime | When the light should turn on each day | 25200<br /> *(7 AM)* | seconds after midnight |
| lightInterval | Amount of time the light should run for each day | 43200<br /> *(12 hours)* | seconds |

## Known Issues

- The moisture sensor currently used is basically worthless. It's not used to determine when or for how long to water. Its values are just displayed in the web GUI while I think of how to make any use of it. The temperature readout is nice.
- If the system is turned off for a few days, and is turned on after its scheduled watering time, it'll only pick up watering on the next day. I'd like to fix this (without risking a flood if the system starts crashing in a loop)
- The web GUI should list more vital statistics like when the plant was last watered
- The system currently has no way to know if it has run out of water