# sensor.py (ESPhome 2025.11-only)
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor as sensor_component
from esphome.const import CONF_NAME, CONF_ID

from . import bm2_ble_ns, BM2BLEComponent

CONF_ROLE = "role"
ROLES = {"voltage", "battery"}

# Use the new 2025.11 sensor schema helper (no fallback)
PLATFORM_SCHEMA = sensor_component.sensor_schema(
    BM2BLEComponent,
    {
        cv.Required(CONF_ID): cv.use_id(BM2BLEComponent),
        cv.Optional(CONF_NAME): cv.string,
        cv.Optional(CONF_ROLE, default="voltage"): cv.one_of(*sorted(ROLES), upper=False),
    },
)


def to_code(config):
    hub = cg.get_variable(config[CONF_ID])
    sens = sensor_component.new_sensor(config)

    # map role to the correct setter
    role = config.get(CONF_ROLE, "voltage")
    if role == "voltage":
        cg.add(hub.set_voltage_sensor(sens))
    else:
        cg.add(hub.set_battery_sensor(sens))

    cg.add(sens)
