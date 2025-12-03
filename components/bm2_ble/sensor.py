# custom_components/bm2_ble/sensor.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor as esphome_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS,
)
from . import bm2_ble_ns, BM2BLEComponent, CONF_BM2_BLE_ID

CONF_ROLE = "role"
ROLES = {"voltage", "battery"}

# create a platform sensor class: sensor + pollingcomponent as in the example pattern
BM2BleSensor = bm2_ble_ns.class_("BM2BleSensor", esphome_sensor.Sensor, cg.PollingComponent)

# use the 2025.11 sensor_schema helper to get all standard options
CONFIG_SCHEMA = esphome_sensor.sensor_schema(
    unit_of_measurement=CONF_UNIT_OF_MEASUREMENT, accuracy_decimals=1
).extend(
    {
        cv.GenerateID(): cv.declare_id(BM2BleSensor),
        cv.Required(CONF_BM2_BLE_ID): cv.use_id(BM2BLEComponent),
        cv.Required(CONF_ROLE): cv.one_of(*sorted(ROLES), lower=True),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esphome_sensor.register_sensor(var, config)
    parent = await cg.get_variable(config[CONF_BM2_BLE_ID])
    # call parent's add_entity(role, var)
    cg.add(parent.add_entity(config[CONF_ROLE], var))
