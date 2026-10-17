# Hosyond 4.0-inch CYD port of Eiswolfs Flightradar

Target: ESP32-WROOM-32E with ST7796S 320x480 TFT and XPT2046 touchscreen.
Board pinout reused from [the CYD40 SquachWatch port](https://github.com/mjwgeek/SquachWatch-CYD-4.0).

Build: `pio run -e hosyond-cyd40`. GitHub Actions builds this branch and provides
bootloader, partition-table, and firmware application binaries separately.

The original portrait 240x320 UI is adapted to **native portrait 320x480**.
Display: CS15, DC2, SCLK14, MOSI13, MISO12, BL27.
Touch: CS33, IRQ36, shared display SPI wires. Run on-screen calibration.
MicroSD: CS5, CLK18, MISO19, MOSI23 (FAT32 card required).
RGB LED: R22, G16, B17. Speaker: GPIO26 audio, GPIO4 active-low amplifier.
GPIO34 is battery ADC, not the original LDR; auto brightness is disabled.
Touch calibration is isolated as `calibration_cyd40.txt`.
**Upstream OTA updates are blocked** on this board pending CYD40 release support.
The original `esp32dev` build is unchanged.

## Flash caution

Do not install the `firmware.bin` at the upstream repository root, or use the
upstream 2.8-inch web flasher: neither image targets the Hosyond display.

Once CI builds pass, a complete three-image installation can use 0x1000
(bootloader.bin), 0x8000 (partitions.bin), and 0x10000 (firmware.bin).
Check partition compatibility and preserve NVS if reusing saved settings.

**Status:** initial port; verify compilation, physical display, touch calibration,
SD card, audio, menus and radar layout on real hardware before calling it tested.


## GitHub Releases

**Download the current Hosyond 4.0 preview:**
https://github.com/mjwgeek/eiswolfs-flightradar-CYD/releases

Only two `.bin` files appear in Releases:

- `Flightradar-Hosyond-4.0-Launcher.bin`: application image, for SD-card
  launchers with a compatible existing app partition and enough space.
- `Flightradar-Hosyond-4.0-Full.bin`: complete merged ESP32 image,
  flashed at offset `0x0` by a compatible serial flash tool.

The three components `bootloader.bin`, `partitions.bin` and `firmware.bin`
are still compiled internally, but aren't public Release assets.

**Warning:** The merged image changes flash layout and can erase saved
settings. The Launcher image is not the correct input for a complete
blank-flash installation. This is a hardware-untested preview, not an
established tested release.

## CYD40 touch and color fix

The XPT2046 touch chip and ST7796S display share one SPI bus. The 4.0 build
now reads touch pressure and raw X/Y through TFT_eSPI (TOUCH_CS=33) rather
than initializing an independent XPT2046 SPI host that can corrupt the LCD
bus. Existing display/touch rotation and saved touch calibration remain
available. Touch sensitivity and the final coordinates require hands-on
validation with the 4-inch board.

The CYD40 inversion baseline is reversed relative to the original 2.8-inch
panel, at the *runtime* invertDisplay() calls (not just the TFT setup macro).
This also fixes SD cards storing the previous invert=1 value. The original
2.8-inch firmware keeps its previous behavior.
