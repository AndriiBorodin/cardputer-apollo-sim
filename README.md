# Apollo AGC // Lunar Flight Simulator for M5Stack Cardputer ADV

> **Retro-instrumental Apollo Guidance Computer (DSKY) & Earth–Moon Three-Body Orbital Simulator**  
> Built specifically for the **M5Stack Cardputer / Cardputer ADV** (`240×135` Landscape LCD, 56-key matrix keyboard, ESP32-S3FN8, I2S Speaker, WS2812 RGB LED).

[![GitHub Pages](https://img.shields.io/badge/GitHub%20Pages-Live%20Manual-brightgreen?logo=github)](https://andriiborodin.github.io/cardputer-apollo-sim/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Build%20Passed-blue?logo=platformio)](platformio.ini)
[![Hardware](https://img.shields.io/badge/Device-M5Stack%20Cardputer-orange)](https://m5stack.com)
[![M5Burner](https://img.shields.io/badge/M5Burner-Official%20Firmware-red?logo=m5stack)](https://burner.m5stack.com/share/firmware/4CKUPV)

---

## 🌐 Live Interactive Flight Manual & Web Academy

📖 **Online Web Simulator & Flight Manual**:  
👉 **[https://andriiborodin.github.io/cardputer-apollo-sim/](https://andriiborodin.github.io/cardputer-apollo-sim/)**

Explore the full interactive bilingual (**UA / EN**) flight manual directly in your browser:
- **Interactive Cardputer Display Showcase**: Live interactive screen previews with **authentic BMP screenshots** taken directly on real Cardputer hardware (`[MAP]` orbital radar, `[DSKY]` flight cockpit, and `Smart OS` tile menu).
- **🏛️ Apollo 1969 vs 2025 Hardware Comparison**: Comprehensive side-by-side analysis of the original 32 kg Apollo Guidance Computer vs the 92 g ESP32-S3 Cardputer.
- **Flight Academy**: Step-by-step guidance on Saturn V staging, Trans-Lunar Injection (TLI), Free-Return figure-8 trajectory, and aerodynamic Earth reentry.

---

## 🔥 Quick Flashing via M5Burner (1-Click Install)

You don't need to install PlatformIO or compile code! The project is officially published on **M5Burner**:

👉 **[Install via M5Burner: Apollo AGC Lunar Flight Simulator](https://burner.m5stack.com/share/firmware/4CKUPV)**

### Method A: M5Burner (Recommended - 1 Click)
1. Open the official firmware page: **[https://burner.m5stack.com/share/firmware/4CKUPV](https://burner.m5stack.com/share/firmware/4CKUPV)** (or search for `Apollo AGC` in the M5Burner desktop app under Cardputer).
2. Connect your **M5Stack Cardputer** via USB-C.
3. Click **Burn** — done!

### Method B: Manual Web Flasher (ESP Web Tools)
1. Download the merged flash image: [`release/cardputer_apollo_sim_merged_8mb.bin`](release/cardputer_apollo_sim_merged_8mb.bin).
2. Connect your Cardputer via USB-C (hold `G0` while plugging in or turning on to enter download mode).
3. Open [ESP Web Tools](https://espressif.github.io/esptool-js/) or [Web ESPHome](https://web.esphome.io/).
4. Select `Flash Address: 0x0` and upload `cardputer_apollo_sim_merged_8mb.bin`.

### Method B: PlatformIO (Build from source)
```bash
# Clone the repository
git clone https://github.com/AndriiBorodin/cardputer-apollo-sim.git
cd cardputer-apollo-sim

# Compile and upload to Cardputer
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor
```

---

## 🖥️ Interface & Screens (`240×135` True Landscape)

### 1. Smart OS Main Menu (4 Carousel Tiles)
Navigate tiles with `A` / `D` or `TAB`, press `ENTER` to open a tile, `W` / `S` to select an option, and `DEL` to go back:

1. **1. FLIGHT MODES**:
   - `HOUSTON (DIRECTOR)`: Houston flight director assist! Computer automatically controls trajectory, SAS attitude hold, 85 km MECO, and calculates burn windows. Player issues `GO` commands with `ENTER` and separates stages with `SPACE` when signaled by the hardware RGB LED and screen prompts.
   - `SEMI-AUTO (AGC PILOT)`: Astronaut piloting with SAS stabilization. The computer maintains optimal flight vectors while the player cycles AGC programs (`N`), commands ignitions (`ENTER`), and separates spent stages (`SPACE`).
   - `AUTONOMOUS DEMO FLIGHT`: Full hands-off autonomous mission from pad to ocean splashdown. Only Time Warp (`1`–`5`), Map view/zoom/camera (`TAB`/`M`, `+`/`-`, `C`, `;` `.` `,` `/`), and Exit (`DEL`) are active; unauthorized flight keys trigger a 3-second notice modal.
2. **2. PHYSICS & TIME**:
   - `ARCADE (FAST)`: Fast-paced dynamic scale (~1:10), ~3–5 minute full flight, Time Warp up to `300x`.
   - `REALISTIC 1:1`: Full astronomical scale (Earth $R=6,371\text{ km}$, Moon $d=384,400\text{ km}$, $v_1=7.85\text{ km/s}$), multi-day flight (~140h sim time), Time Warp up to `5000x`.
3. **3. AUDIO SETTINGS**:
   - `SOUND: ON`: Authentic NASA Quindar transmission tone (`2524 Hz`), staging clicks, and Master Alarm buzzer.
   - `SOUND: MUTE`: Silent cockpit operation.
4. **4. DISPLAY THEME**:
   - `APOLLO CRT GREEN`, `AMBER TERMINAL`, `CYAN AVIONICS`, `RED/ORANGE ALERT`.

---

## 🚀 4-Stage Saturn V Architecture

| Display | Stage Name | Role in Mission |
|---|---|---|
| `STG 1 FL` | **S-IC (1st Stage)** | Liftoff and lower atmospheric boost (burns from 0 km to ~22 km in Arcade / ~50 km in Realistic). |
| `STG 2 FL` | **S-II (2nd Stage)** | Upper atmospheric ascent and circularization into 85 km Low Earth Orbit (LEO). |
| `STG 3 FL` | **S-IVB + CSM (3rd Stage)** | Deep-space propulsion for Trans-Lunar Injection (`P15 TLI`), Lunar Orbit Insertion (`P21 LOI`), and Trans-Earth Injection (`P30 TEI`). |
| `STG 4 FL` | **Apollo Command Module (CM)** | Unpowered crew capsule with ablative heatshield. **Must jettison SM before atmospheric entry (`[SPACE]` or `P63`)!** Dual parachutes deploy automatically (<550 m/s drogue, <120 m/s main). |

---

## 💡 Hardware RGB LED Lighting Guide (GPIO 21)

The Cardputer's built-in WS2812 addressable RGB LED guides the player through each mission phase at a glance:

| LED Color & Pattern | Flight State | Player Action Required |
|---|---|---|
| 🟡 **Pulsing Yellow (1.5 Hz)** | Action Authorization Awaiting | Press **`[ENTER]`** (`GO FOR LAUNCH`, `TLI BURN`, `LOI BURN`, `TEI BURN`, `REENTRY`) |
| 🟠 **Flashing Orange (3 Hz)** | Stage Depleted / Burnout | Press **`[SPACE]`** to discard spent rocket stage |
| 🟢 **Solid Green** | Active Engine Burn | Monitoring burn progress (brightness scales with throttle) |
| 🔵 **Soft Blue Breathing** | Deep Space Coasting | Coasting in vacuum orbit / translunar transfer |
| 🔶 **Plasma Orange** | Atmospheric Entry | Aerobraking heatshield shockwave active |
| 🔷 **Sky Blue** | Parachute Descent | Drogue (7 km) and main (2.5 km) parachutes deployed |
| 🔴 **Rapid Red Strobe (5 Hz)** | Master Alarm / Crash | Critical emergency or surface impact abort |
| 🟢✨ **Emerald & Cyan Pulse** | Splashdown Victory | Mission accomplished safely in the ocean! |

---

## ⌨️ Complete Keymap Reference

| Key | Action |
|---|---|
| `TAB` or `M` or `G0` | Toggle between **DSKY Cockpit** and **Earth $\to$ Moon Map** |
| `ENTER` | Authorize `GO` (Houston) / Ignite Engine (Semi-Auto) / Select Tile Menu |
| `SPACE` | Jettison spent stage (`STG 1` $\to$ `STG 2` $\to$ `STG 3` $\to$ `STG 4 CM`) |
| `N` | Cycle next AGC Program (`P11` $\to$ `P15` $\to$ `P20` $\to$ `P21` $\to$ `P30` $\to$ `P63`) |
| `1` `2` `3` `4` `5` | Time Warp (`1x`..`300x` in Arcade, `1x`..`5000x` in Realistic) |
| `+` / `-` (or `I` / `O`) | Map Zoom In / Zoom Out (`0.4x` to `35x`) |
| `C` | Cycle Map Camera Focus (`SYS` $\to$ `SHIP` $\to$ `EARTH` $\to$ `MOON`) |
| `;` `.` `,` `/` | Pan Map camera (Up / Down / Left / Right) |
| `DEL` / `BACKSPACE` | Abort Mission Confirmation Modal / Back in Menu |

---

## 📄 License & Credits

Created by **Andrii Borodin** ([@AndriiBorodin](https://github.com/AndriiBorodin)).  
Inspired by NASA's Apollo 11 Mission, MIT Instrumentation Laboratory, and Margaret Hamilton's flight software team.
