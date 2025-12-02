import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_MAC_ADDRESS, CONF_UPDATE_INTERVAL
from esphome.components import sensor, text_sensor
from esphome import automation

AUTO_LOAD = True

bm2_ble_ns = cg.esphome_ns.namespace('bm2_ble')
BM2BLEComponent = bm2_ble_ns.class_('BM2BLEComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_MAC_ADDRESS): cv.string,
    cv.Optional(CONF_UPDATE_INTERVAL, default=10): cv.positive_time_period_seconds,
    cv.GenerateID(): cv.declare_id(BM2BLEComponent),
}, extra=cv.ALLOW_EXTRA)

def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    cg.add(var.set_mac(config[CONF_MAC_ADDRESS]))
    cg.add(var.set_update_interval(config.get(CONF_UPDATE_INTERVAL)))
    cg.add(var)
