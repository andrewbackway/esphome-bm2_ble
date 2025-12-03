# custom_components/bm2_ble/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_UPDATE_INTERVAL, CONF_ID
from esphome.components import ble_client

AUTO_LOAD = True

bm2_ble_ns = cg.esphome_ns.namespace("bm2_ble")
BM2BLEComponent = bm2_ble_ns.class_("BM2BLEComponent", cg.Component)

# constant for platform usage
CONF_BM2_BLE_ID = "bm2_ble_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BM2BLEComponent),
        cv.Required("ble_client_id"): cv.use_id(ble_client.BLEClient),
        cv.Optional(CONF_UPDATE_INTERVAL, default=10): cv.positive_time_period_seconds,
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    ble = await cg.get_variable(config["ble_client_id"])
    cg.add(var.set_ble_client(ble))
    cg.add(var.set_update_interval(config.get(CONF_UPDATE_INTERVAL)))
    # registering the component is enough; there's no need to add the var by itself
    # return the var implicitly for further use
