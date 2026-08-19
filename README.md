# 🎵 VinylChroma — Let Your Vinyl Shine 🌈

**Stable release: v1.0.0**

VinylChroma is ESP32-S3 firmware that measures the color of a vinyl record with
up to four TCS34725 sensors and sends the accepted color to a separate
[WLED](https://github.com/wled/WLED) controller over its HTTP JSON API. The
VinylChroma controller handles sensing,
calibration, averaging, presence detection, and output decisions; it does not
drive the addressable LED strip data line itself.

### **Bring your vinyl colors to life and let them set the mood — from a subtle turntable glow to lighting up your room or your entire home.**

## ✨ Features

- One to four independently configurable TCS34725 color sensors
- Direct I²C operation for one sensor or TCA9548A multiplexing for two to four
- Automatic, shared-manual, and per-sensor gain/integration control
- Per-sensor white calibration with dark/white light references and optional RGB fine-tuning
- Instant, rolling-time, and turntable-revolution averaging
- Vinyl presence detection through the exposure-normalized Clear channel
- Configurable color acceptance, default-color, and WLED-off timers
- Adjustable output Value normalization after sensor weighting and averaging
- Optional calibrated darkness cutoff for very dark detected vinyl colors
- WLED segment selection, optional brightness transmission, and optional effect preservation
- Static color overrides plus RGB Rainbow and Marble test effects
- RAM-only color history, diagnostics, logs, and live sensor information
- Embedded responsive web interface with validation and concise help text
- Configuration backup/restore, browser-based OTA updates with a persistent allow/deny switch, and factory reset
- Optional HTTP Basic Authentication for the web interface and built-in APIs

## 🧩 Supported Hardware

| Component | Quantity | Notes |
| --- | ---: | --- |
| ESP32-S3 SuperMini | 1 | Project profile assumes 4 MB QIO flash and native USB |
| TCS34725 breakout | 1–4 | Fixed I²C address `0x29` |
| TCA9548A multiplexer | 0 or 1 | Required when using two or more TCS34725 sensors; address `0x70` |
| Controllable sensor illumination | 1 per sensor | Driven through the configured LED-control GPIO |
| WLED controller and LED strip | 1 | Separate network device running WLED |

> VinylChroma supports TCS34725 sensors. TCS3200 sensors use a frequency-output
> interface and are not compatible with this firmware.

Use 3.3 V logic with the ESP32-S3 and verify the electrical requirements of the
specific sensor breakout. All devices must share ground. Do not drive a lamp or
high-current LED load directly from an ESP32 GPIO; use the breakout's logic-level
LED control or a suitable driver circuit.

## 🔌 Default Wiring

| Signal | ESP32-S3 pin | Function |
| --- | --- | --- |
| GND | GND | Common ground |
| Sensor supply | 3.3 V | Sensor/multiplexer supply when supported by the breakout |
| SDA | GPIO12 | Shared I²C data |
| SCL | GPIO13 | Shared I²C clock |
| Sensor 1 illumination | GPIO11 | PWM control |
| Sensor 2 illumination | GPIO10 | PWM control |
| Sensor 3 illumination | GPIO9 | PWM control |
| Sensor 4 illumination | GPIO8 | PWM control |

With no TCA9548A detected, only **Sensor 1** can be used directly on the I²C
bus. With a TCA9548A, connect all active TCS34725 sensors behind the
multiplexer. The factory channel assignment is 0–3 for Sensors 1–4; the web
interface allows four unique channels from 0–7. Do not combine a direct-bus
TCS34725 with multiplexed TCS34725 sensors.

The configurable ESP32-S3 GPIO allowlist is `1`, `2`, `4–18`, and `21`. GPIO3
is avoided because it is a strapping pin, while GPIO19 and GPIO20 are reserved
for native USB. SDA, SCL, and all four illumination GPIOs must be unique.

## 🏗️ 3D Printable Enclosure

I designed a dedicated 3D-printable enclosure for VinylChroma that provides space for the ESP32-S3 SuperMini and the TCS34725 color sensor, including the sensor light shield and PCB mounting features.

The firmware and source code remain freely available here on GitHub. The optional enclosure STL files are available separately on Cults3D.

If you like this project and would like to support its development, purchasing the enclosure model would be greatly appreciated!

#### **[Get the VinylChroma enclosure on Cults3D](CULTS3D_URL)**

## 🚀 Getting Started
### 📦 Build Requirements

- [PlatformIO](https://platformio.org/) Core CLI or the PlatformIO IDE extension
- A USB data cable for the initial flash
- The project files from this repository

Dependencies are pinned in `platformio.ini` and are installed automatically by
PlatformIO.

### ⚡ Build and Initial USB Flash

From the repository root:

```powershell
pio run -e esp32-s3-supermini
pio run -e esp32-s3-supermini -t upload
```

If automatic serial-port detection fails, specify the actual port:

```powershell
pio device list
pio run -e esp32-s3-supermini -t upload --upload-port COM5
```

Replace `COM5` with the port reported for your controller, for example
`/dev/ttyACM0` on Linux.

The firmware image is generated at:

```text
.pio/build/esp32-s3-supermini/firmware.bin
```

The web interface is embedded in the firmware. No filesystem image or
`uploadfs` step is required. A clean build is normally unnecessary; use
`pio run -t clean` only when investigating stale artifacts or toolchain issues.

### 📶 First Start and Factory Defaults

The following access point is created after a factory reset or when no station
Wi-Fi configuration is available:

| Setting | Default |
| --- | --- |
| SSID | `VinylChroma` |
| Password | `vinyl!1234` |
| Web interface | `http://192.168.4.1` |

Connect to the access point, open the web interface, and configure the target
Wi-Fi network. After joining the network, VinylChroma is available at its DHCP
address and usually at `http://vinylchroma.local`. If station connection fails,
the fallback access point starts again after the configured delay when it is
enabled.

Change the public default fallback-AP password during setup. Web Access
Protection is disabled by default and should be enabled when other clients can
reach the device.

### ⚙️ Initial Configuration

1. Open **Sensors** and verify the I²C GPIOs, illumination GPIOs, and TCA channels.
2. Enable the installed sensors and confirm they are detected on the Dashboard.
3. Set the illumination brightness and exposure mode.
4. Place a neutral matte white reference beneath every active sensor and run
   **White Calibration**. Keep it in place while VinylChroma first captures the
   LED-off baseline and then the illuminated white reference.
5. Open **WLED**, enter the WLED host without `http://`, then set its port and segment.
6. Configure presence detection, averaging, acceptance delay, and absence timers under **Vinyl**.

Recalibrate after changing sensor geometry, illumination brightness, LED
correction, TCA wiring, or an individual sensor.

## WLED 🔵🔴🟠🟡🟢
### 🎨 WLED Output Options

- **WLED Transmission Enabled** sends accepted colors and automatic on/off commands.
- **Send Brightness** includes the configured global WLED brightness; when disabled,
  VinylChroma leaves WLED brightness unchanged.
- **Keep Selected WLED Effect** omits the command that selects Solid mode. The active
  WLED effect may ignore or reinterpret the incoming color.

WLED transitions, gamma correction, current limiting, color order, and RGBW
behavior remain WLED settings.

### 🔦 Darkness Cutoff

After White Calibration, the **Vinyl** page shows a relative light level from
`0%` at the stored LED-off baseline to `100%` at the illuminated white
reference. This is a relative sensor value, not laboratory lux or reflectance.

**Turn WLED Off for Dark Readings** sends an actual WLED off command when the
level of an already detected record remains below the configured threshold.
Presence detection and absence timers are not changed, and debug overrides and
effects bypass the cutoff. The option is disabled by default.


## 📡 OTA Updates

VinylChroma supports firmware updates both through the web interface and directly from PlatformIO over Wi-Fi.

### 🌐 Browser OTA
Browser-based firmware uploads are allowed by default. Under
**System → Firmware OTA**, keep **Allow Firmware OTA Updates** enabled and save
the setting before selecting `firmware.bin`.

A successful update reboots the controller automatically.

Disable the checkbox when OTA is not needed. The `/update` endpoint then rejects
firmware uploads before writing to flash. This setting does not disable USB
flashing, so OTA can still be restored with a USB upload if web access is lost.

### >_ PlatformIO OTA

For later firmware updates over Wi-Fi, use the dedicated OTA environment:

```ini
[env:esp32-s3-supermini-ota]
upload_protocol = espota
upload_port = 192.168.0.25
```

From the repository root:

```powershell
pio run -e esp32-s3-supermini-ota
pio run -e esp32-s3-supermini-ota -t upload
```

If the controller uses a different IP address, specify it directly:

```powershell
pio run -e esp32-s3-supermini-ota -t upload --upload-port 192.168.0.25
```

Replace `192.168.0.25` with the current IP address of your VinylChroma controller.

The controller must already be connected to the same network and the PlatformIO OTA service must be running before an `espota` upload can be performed.

The firmware image is generated at:

```text
.pio/build/esp32-s3-supermini-ota/firmware.bin
```

The web interface is embedded in the firmware. No filesystem image or
`uploadfs` step is required.

A clean build is normally unnecessary. Use:

```powershell
pio run -e esp32-s3-supermini-ota -t clean
```

only when investigating stale artifacts or toolchain issues.
</details>


## 💾 Persistence

### Stored in NVS

Preserved across normal reboots and firmware uploads:

- Wi-Fi and fallback-AP configuration
- GPIO, sensor, averaging, timer, and WLED settings
- White/dark calibration references and manual per-sensor corrections
- Web authentication and OTA-permission configuration

### RAM-only Data

Cleared on every reboot:

- Color History
- Debug overrides and simulations
- Runtime logs

### Configuration Backup

Configuration imports are limited to 16 KB. Exports include stored Wi-Fi,
fallback-AP, and web-login passwords, so treat exported JSON files as secrets.

### 🔄 Flash Behavior During Updates

Normal USB, browser OTA, and PlatformIO OTA uploads using the same partition
layout preserve settings and calibration.

A full flash erase, factory reset, or incompatible partition-table change can
remove or invalidate stored data.

A full flash erase cannot be performed over OTA. Use the USB environment and a
serial connection when `erase` is required.


## 🔐 Security

- The web interface, API, OTA upload, and WLED requests use unencrypted HTTP.
- Optional Web Access Protection uses HTTP Basic Authentication, not TLS.
- OTA images are not signed, and firmware authenticity is not verified.
- OTA uploads are enabled by default and can be disabled under **System**.
- The WLED client does not support WLED authentication or HTTPS.
- Use VinylChroma only on a trusted local network or an appropriately isolated VLAN.

## 🎛️ Configuration Ranges & Tweaks

| Setting | Range |
| --- | --- |
| Rolling window | 0.1–300.0 s |
| RPM | 1–1000 |
| Revolutions | 0.1–20.0 |
| Clear threshold | 0–65535 |
| Required sensors | 1–4 |
| Color-change threshold | 0–764 |
| Color-acceptance delay | 0.0–300.0 s |
| Default-color and WLED-off delays | 0–300 s |
| Output Value normalization | 0–100% |
| Darkness cutoff | 0.0–100.0% (disabled by default) |

## 🛠️ Troubleshooting

- If the updated GUI looks stale, perform a hard refresh (`Ctrl+F5`).
- If Wi-Fi cannot connect, wait for the fallback access point when it is enabled
  and use it to correct the station credentials.
- If neither station Wi-Fi nor the fallback access point can be reached, connect
  through USB, run `pio run -e esp32-s3-supermini -t erase`, and upload the
  firmware again. This erases all stored settings and calibration data.
- If sensors are missing, first enable **System → Developer Mode**, then use
  **Diagnostics → Refresh / Scan I²C** and verify `0x29`/`0x70` wiring.
- If WLED shows a different result, check its active effect, transition, brightness,
  RGB/RGBW mode, color order, gamma correction, and power limiting.
- Do not erase flash or clean the build unless a specific problem requires it.

## 📁 Project Structure

| Path | Purpose |
| --- | --- |
| `include/` | Configuration, hardware, state, and component interfaces |
| `src/` | Firmware implementation and embedded web interface |
| `platformio.ini` | Board profile, build flags, partition layout, and dependencies |
| `.pio/` | Generated local build output; excluded from Git |

## 📄 License

VinylChroma is source-available under the
[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0).

Required Notice: Copyright 2026 Gittegatt

Personal, educational, research, hobby, and other noncommercial use is
permitted subject to the license terms. Commercial use requires a separate
written license from the copyright holder. Because commercial use is
restricted, this is not an OSI-approved open-source license. Third-party
components remain subject to their respective licenses.

## ✉️ Contact

For project information and commercial licensing inquiries, visit
[github.com/Gittegatt/VinylChroma](https://github.com/Gittegatt/VinylChroma)
