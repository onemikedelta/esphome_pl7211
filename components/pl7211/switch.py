import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import pl7211_ns, CONF_PL7211_ID, PL7211Component

CONF_BIT = "bit"
PL7211Switch = pl7211_ns.class_('PL7211Switch', switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(PL7211Switch).extend({
    cv.GenerateID(CONF_PL7211_ID): cv.use_id(PL7211Component),
    cv.Required(CONF_BIT): cv.int_range(min=0, max=15),
})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    
    hub = await cg.get_variable(config[CONF_PL7211_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_bit(config[CONF_BIT]))