# ESPHome SPL06-007 External Component

An ESPHome external component for the **Goertek SPL06-007** (and SPL06-001) barometric pressure and temperature sensor over I2C.

This component was built because the SPL06 chip has no native ESPHome support, and is commonly found on cheap **GY-BMEP** breakout boards sold as BMP280/BME280 — but with a different chip silkscreened on the PCB.

## Supported Hardware

- **SPL06-007** (Goertek) — commonly found on GY-BMEP boards
- **SPL06-001** (Goertek) — same register set
- Any breakout board using the SPL06 with I2C interface

**I2C Addresses:** `0x76` (default) or `0x77`

## Features

- Temperature reading (°C)
- Barometric pressure reading (hPa)
- 8x oversampling for both temperature and pressure
- Full factory calibration coefficient compensation
- Chip ID verification on startup
- Configurable update interval

## Installation

Copy the `components/spl06/` folder into your ESPHome config directory:

```
config/esphome/
  components/
    spl06/
      __init__.py
      sensor.py
      spl06.h
      spl06.cpp
  your-device.yaml
```

Then add this to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: local
      path: components
```

## Wiring

```
SPL06 / GY-BMEP          ESP Board
───────────────           ─────────
VCC  ──────────────────── 3.3V
GND  ──────────────────── GND
SDA  ──────────────────── SDA (e.g. D2 / GPIO4)
SCL  ──────────────────── SCL (e.g. D1 / GPIO5)
```

Works with ESP8266 (D1 Mini, NodeMCU, etc.) and ESP32 boards.

## Configuration

```yaml
i2c:
  sda: D2
  scl: D1

sensor:
  - platform: spl06
    temperature:
      name: "Temperature"
    pressure:
      name: "Pressure"
    address: 0x76
    update_interval: 60s
```

### Configuration Variables

- **temperature** (*Optional*): Temperature sensor.
  - **name** (**Required**, string): The name of the sensor.
  - All other options from [Sensor](https://esphome.io/components/sensor/).
- **pressure** (*Optional*): Pressure sensor.
  - **name** (**Required**, string): The name of the sensor.
  - All other options from [Sensor](https://esphome.io/components/sensor/).
- **address** (*Optional*, int): I2C address. Defaults to `0x76`. Try `0x77` if not detected.
- **update_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): How often to read the sensor. Defaults to `60s`.

## Full Example

```yaml
esphome:
  name: weather-sensor
  friendly_name: Weather Sensor

esp8266:
  board: d1_mini

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
logger:
ota:
  platform: esphome

external_components:
  - source:
      type: local
      path: components

i2c:
  sda: D2
  scl: D1

sensor:
  - platform: spl06
    temperature:
      name: "Temperature"
    pressure:
      name: "Pressure"
    address: 0x76
    update_interval: 60s
```

## How It Works

1. Verifies the chip ID register (expects `0x10` for SPL06)
2. Reads 18 bytes of factory calibration coefficients from registers `0x10`-`0x21`
3. Configures 8x oversampling for both pressure and temperature
4. Sets continuous measurement mode
5. On each update cycle, reads raw 24-bit values and applies the calibration polynomial:

   - **Temperature:** `(c0 * 0.5) + (c1 * traw_scaled)`
   - **Pressure:** Compensated using all 9 coefficients (c00, c10, c20, c30, c01, c11, c21 + temperature cross-compensation)

## Troubleshooting

**"Unknown chip ID" error:**
Your board might have a different sensor (BMP280, BME280). Check the chip marking on the PCB. The SPL06 expects chip ID `0x10`.

**"No device found at address" error:**
Try swapping `address: 0x76` to `address: 0x77`. If your board has an SDO pin, tying it to GND gives `0x76`, tying to 3.3V gives `0x77`.

**"Coefficients not ready" error:**
The sensor may need more time after reset. This is rare — try power cycling the board.

**Temperature reads a few degrees high:**
If your sensor is mounted near an ESP chip, the heat will skew readings. Mount the sensor away from the microcontroller or use a case with thermal separation.

## Is My Board an SPL06?

Many GY-BMEP boards ship with an SPL06 instead of the advertised BMP280/BME280. If ESPHome's `bmp280_i2c` component gives you `Wrong chip ID`, you likely have an SPL06. Check the small text printed on the chip itself.

## License

MIT

## Acknowledgements

- Register map and calibration math based on the [SPL06-007 datasheet](https://datasheet.lcsc.com/szlcsc/1912111437_Goertek-SPL06-007_C233787.pdf)
- Coefficient extraction logic referenced from [rv701/SPL06-007](https://github.com/rv701/SPL06-007) and [happy12/SPL06-001](https://github.com/happy12/SPL06-001) Arduino libraries
