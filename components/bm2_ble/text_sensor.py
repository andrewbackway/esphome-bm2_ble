import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_NAME, CONF_ID

from . import bm2_ble_ns, BM2BLEComponent

PLATFORM_SCHEMA = text_sensor.PLATFORM_SCHEMA.extend({
    cv.Required(CONF_ID): cv.use_id(BM2BLEComponent),
    cv.Optional(CONF_NAME): cv.string,
})

def to_code(config):
    hub = cg.get_variable(config[CONF_ID])
    txt = text_sensor.new_text_sensor(config)
    cg.add(hub.set_status_text(txt))
    cg.add(txt)
