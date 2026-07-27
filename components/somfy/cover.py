import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import (
    CONF_ID,
)
from . import somfy_ns, SomfyCover

CONF_REMOTEID_KEY = 'RemoteID'
CONF_INVERT_KEY = 'invert'

CONFIG_SCHEMA = cover.cover_schema(SomfyCover).extend(
    {
        cv.Required(CONF_REMOTEID_KEY): cv.int_,
        cv.Optional(CONF_INVERT_KEY, default=False): cv.boolean, # type: ignore
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    cg.add(var.setCoverID(config[CONF_REMOTEID_KEY]))
    cg.add(var.setInvert(config[CONF_INVERT_KEY]))
    
    cg.add_library("FS", None)
    cg.add_library("SPI", None)
    cg.add_library("Preferences", None)
    cg.add_library(
        name="CC1101",
        # Patched fork: fixes an uninitialized PA-table value in setPA() and
        # a silently-dropped frequency carry in setMHZ() (both found via
        # -Wmaybe-uninitialized / -Wtype-limits build warnings). See
        # https://github.com/mbaciu/SmartRC-CC1101-Driver-Lib for details.
        repository="https://github.com/mbaciu/SmartRC-CC1101-Driver-Lib",
        version=None
    )