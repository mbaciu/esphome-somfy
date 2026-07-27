import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@Viproz"]

# Top-level `somfy:` block. Configures the CC1101 radio pins shared by every
# `cover: - platform: somfy` entry (they all talk to the same physical chip,
# so this lives here once rather than being repeated per-cover).
#
# Previous versions of this component never called the driver's setSpiPin()/
# setGDO(), so it silently used whichever pins the driver defaulted to for a
# generic "ESP32" target (or, for the uninitialized globals path, GPIO0) --
# fine on the Wemos D1 Mini this was originally written for, wrong on boards
# with a different or smaller pinout (e.g. Seeed XIAO ESP32-S3).
#
# Example:
#   somfy:
#     sck_pin: 7
#     miso_pin: 8
#     mosi_pin: 9
#     csn_pin: 6
#     gdo0_pin: 3
#     gdo2_pin: 4

CONF_SCK_PIN = "sck_pin"
CONF_MISO_PIN = "miso_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_CSN_PIN = "csn_pin"
CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"

# Defaults match the original project's wiring doc (Wemos D1 Mini / plain
# ESP32) so an existing somfy.yaml with no `somfy:` block keeps working
# unchanged.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SCK_PIN, default=18): cv.int_,
        cv.Optional(CONF_MISO_PIN, default=19): cv.int_,
        cv.Optional(CONF_MOSI_PIN, default=23): cv.int_,
        cv.Optional(CONF_CSN_PIN, default=5): cv.int_,
        cv.Optional(CONF_GDO0_PIN, default=2): cv.int_,
        cv.Optional(CONF_GDO2_PIN, default=4): cv.int_,
    }
)


async def to_code(config):
    # SomfyCover.h is pulled into the build by the `cover:` platform; this
    # just needs to run after that header is visible, which it is -- ESPHome
    # generates one translation unit that includes every component's headers.
    cg.add(
        cg.RawExpression(
            "esphome::somfy::SomfyCover::configureRadioPins("
            f"{config[CONF_SCK_PIN]}, {config[CONF_MISO_PIN]}, "
            f"{config[CONF_MOSI_PIN]}, {config[CONF_CSN_PIN]}, "
            f"{config[CONF_GDO0_PIN]}, {config[CONF_GDO2_PIN]})"
        )
    )
