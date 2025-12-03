# custom_components/bm2_ble/text_sensor.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor as text_sensor_component
from esphome.const import CONF_ID, CONF_NAME

from . import bm2_ble_ns, BM2BLEComponent, CONF_BM2_BLE_ID

BM2BleTextSensor = bm2_ble_ns.class_("BM2BleTextSensor", text_sensor_component.TextSensor)

PLATFORM_SCHEMA = text_sensor_component.text_sensor_schema(
    BM2BLEComponent,
    {
        cv.GenerateID(): cv.declare_id(BM2BleTextSensor),
        cv.Required(CONF_BM2_BLE_ID): cv.use_id(BM2BLEComponent),
        cv.Optional(CONF_NAME): cv.string,
    },
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor_component.register_text_sensor(var, config)
    parent = await cg.get_variable(config[CONF_BM2_BLE_ID])
    cg.add(parent.add_entity("status", var))
