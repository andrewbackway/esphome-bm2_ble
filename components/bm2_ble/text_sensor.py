# custom_components/bm2_ble/text_sensor.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, CONF_NAME
from esphome.components import text_sensor as esphome_text_sensor

from . import bm2_ble_ns, BM2BLEComponent, CONF_BM2_BLE_ID

# allowed types/roles for text sensors
CONF_ROLE = CONF_TYPE
ROLES = {"status"}

BM2BleTextSensor = bm2_ble_ns.class_("BM2BleTextSensor", esphome_text_sensor.TextSensor, cg.PollingComponent)

# Use the 2025.11 text_sensor_schema helper and require the hub id + role/type
CONFIG_SCHEMA = esphome_text_sensor.text_sensor_schema(BM2BleTextSensor).extend({
    cv.GenerateID(): cv.declare_id(BM2BleTextSensor),
    cv.Required(CONF_BM2_BLE_ID): cv.use_id(BM2BLEComponent),
    cv.Required(CONF_ROLE): cv.one_of(*sorted(ROLES), lower=True),
}).extend(cv.polling_component_schema('60s'))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esphome_text_sensor.register_text_sensor(var, config)
    parent = await cg.get_variable(config[CONF_BM2_BLE_ID])
    cg.add(parent.add_entity(config[CONF_ROLE], var))
