import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_NAME, CONF_ID
from . import bm2_ble_ns, BM2BLEComponent

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(BM2BLEComponent),
    cv.Optional(CONF_NAME): cv.string,
})

def to_code(config):
    hub = cg.get_variable(config[CONF_ID])
    sensor = cg.new_Pvariable(config.get(CONF_NAME, "bm2_sensor"))
    cg.add(hub.set_voltage_sensor(sensor))
    cg.add(sensor)
