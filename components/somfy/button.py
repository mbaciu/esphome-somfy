import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from . import somfy_ns, SomfyCover

CONF_COVER_ID = "cover_id"
CONF_CMD = "cmd"

SomfyCommandButton = somfy_ns.class_(
    "SomfyCommandButton", button.Button, cg.Component
)

CONFIG_SCHEMA = button.button_schema(SomfyCommandButton).extend(
    {
        cv.Required(CONF_COVER_ID): cv.use_id(SomfyCover),
        cv.Required(CONF_CMD): cv.int_,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_COVER_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_cmd(config[CONF_CMD]))
