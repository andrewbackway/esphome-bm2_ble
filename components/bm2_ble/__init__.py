# custom_components/bm2_ble/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_UPDATE_INTERVAL
from esphome.components import ble_client

AUTO_LOAD = True

bm2_ble_ns = cg.esphome_ns.namespace('bm2_ble')
BM2BLEComponent = bm2_ble_ns.class_('BM2BLEComponent', cg.Component)

# Top-level hub schema: expects a ble_client id and optional update interval
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BM2BLEComponent),
    cv.Required('ble_client_id'): cv.use_id(ble_client.BLEClient),
    cv.Optional(CONF_UPDATE_INTERVAL, default=10): cv.positive_time_period_seconds,
}, extra=cv.ALLOW_EXTRA)


def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    ble = cg.get_variable(config['ble_client_id'])
    # set BLE client reference and update interval on the C++ object
    cg.add(var.set_ble_client(ble))
    cg.add(var.set_update_interval(config.get(CONF_UPDATE_INTERVAL)))
    cg.add(var)
