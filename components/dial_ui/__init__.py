"""dial_ui: the whole Dialed UI, built in C++ on ESPHome's LVGL instance.

See CLAUDE.md (architecture), spec.md (screens) and contract.md (data contract).
"""

import math
from pathlib import Path

import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.lvgl.defines import CONF_LVGL_ID
from esphome.components.lvgl.lvcode import LvglComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_SENSOR
from esphome.core import CORE
from esphome.helpers import copy_file_if_changed

CODEOWNERS = []
DEPENDENCIES = ["lvgl"]

# design/lvgl holds ui_tokens.h, the single source of truth for colours and sizes.
DESIGN_LVGL_DIR = Path(__file__).resolve().parents[2] / "design" / "lvgl"

CONF_ROOMS = "rooms"
CONF_METRIC = "metric"
CONF_MIN = "min"
CONF_MAX = "max"
CONF_METRICS = "metrics"
CONF_THRESHOLDS = "thresholds"
CONF_HISTORY = "history"
CONF_WARN_LOW = "warn_low"
CONF_ALERT_LOW = "alert_low"
CONF_WARN_HIGH = "warn_high"
CONF_ALERT_HIGH = "alert_high"

dial_ui_ns = cg.esphome_ns.namespace("dial_ui")
DialUi = dial_ui_ns.class_("DialUi", cg.Component)
Metric = dial_ui_ns.enum("Metric", is_class=True)
Thresholds = dial_ui_ns.struct("Thresholds")

# Page order on a room (design ScreenSensor) and the C++ enum.
METRICS = {
    "temperature": Metric.TEMPERATURE,
    "humidity": Metric.HUMIDITY,
    "co2": Metric.CO2,
    "pm25": Metric.PM25,
}
METRIC_ORDER = list(METRICS)

# Default ring ranges and status limits (spec.md "Data decisions"). None = limit not used.
THRESHOLD_KEYS = (CONF_MIN, CONF_MAX, CONF_WARN_LOW, CONF_ALERT_LOW, CONF_WARN_HIGH, CONF_ALERT_HIGH)
DEFAULT_THRESHOLDS = {
    "temperature": {CONF_MIN: 10, CONF_MAX: 40, CONF_WARN_LOW: 18, CONF_ALERT_LOW: 16, CONF_WARN_HIGH: 26, CONF_ALERT_HIGH: 30},
    "humidity": {CONF_MIN: 0, CONF_MAX: 100, CONF_WARN_LOW: 40, CONF_ALERT_LOW: 30, CONF_WARN_HIGH: 60, CONF_ALERT_HIGH: 70},
    "co2": {CONF_MIN: 400, CONF_MAX: 2000, CONF_WARN_LOW: None, CONF_ALERT_LOW: None, CONF_WARN_HIGH: 1000, CONF_ALERT_HIGH: 1500},
    "pm25": {CONF_MIN: 0, CONF_MAX: 75, CONF_WARN_LOW: None, CONF_ALERT_LOW: None, CONF_WARN_HIGH: 15, CONF_ALERT_HIGH: 35},
}

ROOM_NAME_MAX = 12


def _validate_thresholds(value):
    lo, hi = value.get(CONF_MIN), value.get(CONF_MAX)
    if lo is not None and hi is not None and not hi > lo:
        raise cv.Invalid("max must be greater than min")
    return value


THRESHOLDS_SCHEMA = cv.All(
    cv.Schema({cv.Optional(k): cv.float_ for k in THRESHOLD_KEYS}),
    _validate_thresholds,
)

READING_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_METRIC): cv.one_of(*METRICS, lower=True),
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        # History pages arrive in M4; accepted now so packages/ui.yaml does not change later.
        cv.Optional(CONF_HISTORY, default=False): cv.boolean,
        cv.Optional(CONF_THRESHOLDS, default={}): THRESHOLDS_SCHEMA,
    }
)


def _validate_room(room):
    seen = [m[CONF_METRIC] for m in room[CONF_METRICS]]
    if len(seen) != len(set(seen)):
        raise cv.Invalid("each metric may appear only once per room", [CONF_METRICS])
    # Pages follow the fixed metric order, whatever the YAML order.
    room[CONF_METRICS] = sorted(room[CONF_METRICS], key=lambda m: METRIC_ORDER.index(m[CONF_METRIC]))
    return room


ROOM_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.string_strict,
            cv.Required(CONF_NAME): cv.All(cv.string_strict, cv.Length(min=1)),
            cv.Required(CONF_METRICS): cv.All(cv.ensure_list(READING_SCHEMA), cv.Length(min=1, max=4)),
        }
    ),
    _validate_room,
)


def _validate_rooms(rooms):
    ids = [r[CONF_ID] for r in rooms]
    if len(ids) != len(set(ids)):
        raise cv.Invalid("room ids must be unique")
    names = [r[CONF_NAME][:9] for r in rooms]
    if len(names) != len(set(names)):
        # design/README.md: names are cut at 12 characters, so rooms must differ in the first 9.
        raise cv.Invalid("room names must differ in their first 9 characters")
    return rooms


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DialUi),
        cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
        # Per-metric overrides of DEFAULT_THRESHOLDS for every room.
        cv.Optional(CONF_THRESHOLDS, default={}): cv.Schema(
            {cv.Optional(m): THRESHOLDS_SCHEMA for m in METRICS}
        ),
        cv.Optional(CONF_ROOMS, default=[]): cv.All(cv.ensure_list(ROOM_SCHEMA), _validate_rooms),
    }
).extend(cv.COMPONENT_SCHEMA)


def _float_expr(value):
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return cg.RawExpression("NAN")
    return cg.RawExpression(f"{float(value)!r}f")


async def to_code(config):
    # Copy the tokens into the build's src/ (on the include path, like lvgl's lv_conf.h).
    # An -I build flag does not work: ESP-IDF builds only forward -D/-W flags
    # (esphome/build_gen/espidf.py), and files added under src/esphome/ are deleted
    # by the source tree sync (esphome/writer.py: copy_src_tree).
    tokens = DESIGN_LVGL_DIR / "ui_tokens.h"
    if not tokens.is_file():
        raise cv.Invalid(f"ui_tokens.h not found in {DESIGN_LVGL_DIR}")
    copy_file_if_changed(tokens, CORE.relative_src_path("ui_tokens.h"))

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_LVGL_ID])

    global_overrides = config[CONF_THRESHOLDS]
    for room in config[CONF_ROOMS]:
        if len(room[CONF_NAME]) > ROOM_NAME_MAX:
            cg.add(cg.RawStatement(f"// room name '{room[CONF_NAME]}' longer than {ROOM_NAME_MAX}: shown with ellipsis"))
        cg.add(var.add_room(room[CONF_ID], room[CONF_NAME]))
        for reading in room[CONF_METRICS]:
            metric = reading[CONF_METRIC]
            t = {**DEFAULT_THRESHOLDS[metric], **global_overrides.get(metric, {}), **reading[CONF_THRESHOLDS]}
            if not t[CONF_MAX] > t[CONF_MIN]:
                raise cv.Invalid(f"room '{room[CONF_ID]}' {metric}: max must be greater than min")
            sens = await cg.get_variable(reading[CONF_SENSOR])
            thresholds = cg.StructInitializer(Thresholds, *[(k, _float_expr(t[k])) for k in THRESHOLD_KEYS])
            cg.add(var.add_reading(METRICS[metric], sens, thresholds))
