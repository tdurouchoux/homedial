"""dial_ui: the whole Dialed UI, built in C++ on ESPHome's LVGL instance.

See CLAUDE.md (architecture), spec.md (screens) and contract.md (data contract).
"""

import math
from pathlib import Path

from esphome import automation
import esphome.codegen as cg
from esphome.components import light, output, sensor, switch, text_sensor, time
from esphome.components.lvgl.defines import CONF_LVGL_ID
from esphome.components.lvgl.lvcode import LvglComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_SENSOR
from esphome.core import CORE
from esphome.helpers import copy_file_if_changed

CODEOWNERS = []
DEPENDENCIES = ["lvgl"]
AUTO_LOAD = ["json", "text_sensor"]  # load_forecast() takes the action's JSON response

# design/lvgl holds ui_tokens.h, the single source of truth for colours and sizes.
DESIGN_LVGL_DIR = Path(__file__).resolve().parents[2] / "design" / "lvgl"

CONF_ROOMS = "rooms"
CONF_METRIC = "metric"
CONF_MIN = "min"
CONF_MAX = "max"
CONF_METRICS = "metrics"
CONF_THRESHOLDS = "thresholds"
CONF_HISTORY = "history"
CONF_TIME_ID = "time_id"
CONF_WEATHER = "weather"
CONF_TEMPERATURE = "temperature"
CONF_CONDITION = "condition"
CONF_RAIN_FORECAST = "rain_forecast"
CONF_RAIN_FORECAST_TIME = "rain_forecast_time"
CONF_ON_FORECAST_REQUEST = "on_forecast_request"
CONF_POWER = "power"
CONF_BACKLIGHT = "backlight"
CONF_BUZZER = "buzzer"
CONF_POWER_HOLD = "power_hold"
CONF_CONTRACT_LIMIT = "contract_limit"
MAX_CAROUSEL_PAGES = 7  # PageDots fit 3 to 7 pages (design/components/PageDots)
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
    "air_quality": Metric.AIR_QUALITY,
}
METRIC_ORDER = list(METRICS)

# Default ring ranges and status limits (spec.md "Data decisions"). None = limit not used.
THRESHOLD_KEYS = (CONF_MIN, CONF_MAX, CONF_WARN_LOW, CONF_ALERT_LOW, CONF_WARN_HIGH, CONF_ALERT_HIGH)
DEFAULT_THRESHOLDS = {
    "temperature": {CONF_MIN: 10, CONF_MAX: 40, CONF_WARN_LOW: 18, CONF_ALERT_LOW: 16, CONF_WARN_HIGH: 26, CONF_ALERT_HIGH: 30},
    "humidity": {CONF_MIN: 0, CONF_MAX: 100, CONF_WARN_LOW: 40, CONF_ALERT_LOW: 30, CONF_WARN_HIGH: 60, CONF_ALERT_HIGH: 70},
    # BME688 BSEC IAQ index: OK up to 100, WARN 101 to 200, ALERT above 200.
    "air_quality": {CONF_MIN: 0, CONF_MAX: 500, CONF_WARN_LOW: None, CONF_ALERT_LOW: None, CONF_WARN_HIGH: 101, CONF_ALERT_HIGH: 201},
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
        # Keep 24 h of hourly means in RAM and give the reading a History page (needs time_id).
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
            cv.Required(CONF_METRICS): cv.All(cv.ensure_list(READING_SCHEMA), cv.Length(min=1, max=3)),
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


# Weather now + hourly forecast (spec.md "Weather", contract.md §6).
WEATHER_SCHEMA = cv.All(
    cv.Schema(
        {
            # Météo-France weather entity: attribute `temperature` and the state (condition).
            cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CONDITION): cv.use_id(text_sensor.TextSensor),
            # Next rain sensor attributes `1_hour_forecast` and `forecast_time_ref`.
            cv.Optional(CONF_RAIN_FORECAST): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_RAIN_FORECAST_TIME): cv.use_id(text_sensor.TextSensor),
            # Runs weather.get_forecasts; on_success hands the rows to id(ui).load_forecast().
            cv.Optional(CONF_ON_FORECAST_REQUEST): automation.validate_automation(single=True),
        }
    ),
    cv.has_none_or_all_keys(CONF_RAIN_FORECAST, CONF_RAIN_FORECAST_TIME),
)


# Electricity (design ScreenPower): live power and the contract limit (ring 0 to limit).
POWER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_CONTRACT_LIMIT): cv.All(cv.float_with_unit("power", "(W|w)?"), cv.Range(min=100.0)),
    }
)


def _validate_carousel(config):
    pages = len(config[CONF_ROOMS]) + (CONF_WEATHER in config) + (CONF_POWER in config)
    if pages > MAX_CAROUSEL_PAGES:
        raise cv.Invalid(
            f"the carousel has {pages} pages (rooms + weather + power); the page dots fit at most "
            f"{MAX_CAROUSEL_PAGES}",
            [CONF_ROOMS],
        )
    return config


def _validate_history_needs_time(config):
    uses_history = any(m[CONF_HISTORY] for r in config[CONF_ROOMS] for m in r[CONF_METRICS])
    if uses_history and CONF_TIME_ID not in config:
        raise cv.Invalid("time_id is required when a metric has history: true", [CONF_TIME_ID])
    if CONF_WEATHER in config and CONF_TIME_ID not in config:
        raise cv.Invalid("time_id is required with weather: (rain timing, forecast hours)", [CONF_TIME_ID])
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DialUi),
        cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
        # Wall clock for history (hour buckets); e.g. the homeassistant time platform.
        cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        # Per-metric overrides of DEFAULT_THRESHOLDS for every room.
        cv.Optional(CONF_THRESHOLDS, default={}): cv.Schema(
            {cv.Optional(m): THRESHOLDS_SCHEMA for m in METRICS}
        ),
        cv.Optional(CONF_ROOMS, default=[]): cv.All(cv.ensure_list(ROOM_SCHEMA), _validate_rooms),
        cv.Optional(CONF_WEATHER): WEATHER_SCHEMA,
        cv.Optional(CONF_POWER): POWER_SCHEMA,
        # Settings targets (spec.md "Settings list"): luminosity, encoder tick, Power off.
        cv.Optional(CONF_BACKLIGHT): cv.use_id(light.LightState),
        cv.Optional(CONF_BUZZER): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_POWER_HOLD): cv.use_id(switch.Switch),
    }
).extend(cv.COMPONENT_SCHEMA)
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_history_needs_time, _validate_carousel)


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
    if CONF_TIME_ID in config:
        cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))

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
            cg.add(var.add_reading(METRICS[metric], sens, thresholds, reading[CONF_HISTORY]))

    if weather := config.get(CONF_WEATHER):
        temp = await cg.get_variable(weather[CONF_TEMPERATURE])
        cond = await cg.get_variable(weather[CONF_CONDITION])
        cg.add(var.set_weather(temp, cond))
        if CONF_RAIN_FORECAST in weather:
            rain = await cg.get_variable(weather[CONF_RAIN_FORECAST])
            rain_time = await cg.get_variable(weather[CONF_RAIN_FORECAST_TIME])
            cg.add(var.set_rain(rain, rain_time))
        if on_request := weather.get(CONF_ON_FORECAST_REQUEST):
            await automation.build_automation(var.get_forecast_request_trigger(), [], on_request)

    if power := config.get(CONF_POWER):
        sens = await cg.get_variable(power[CONF_SENSOR])
        cg.add(var.set_power(sens, power[CONF_CONTRACT_LIMIT]))

    if CONF_BACKLIGHT in config:
        cg.add(var.set_backlight(await cg.get_variable(config[CONF_BACKLIGHT])))
    if CONF_BUZZER in config:
        cg.add(var.set_buzzer(await cg.get_variable(config[CONF_BUZZER])))
    if CONF_POWER_HOLD in config:
        cg.add(var.set_power_hold(await cg.get_variable(config[CONF_POWER_HOLD])))
