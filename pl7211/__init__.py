import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

# Specify that this component requires the SPI bus
DEPENDENCIES = ['spi']
MULTI_CONF = True

# Create a namespace for the C++ code
pl7211_ns = cg.esphome_ns.namespace('pl7211')
PL7211Component = pl7211_ns.class_('PL7211Component', cg.PollingComponent, spi.SPIDevice)

CONF_PL7211_ID = 'pl7211_id'

# Define the configuration schema
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PL7211Component),
}).extend(cv.polling_component_schema('10s')).extend(spi.spi_device_schema(cs_pin_required=True))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)