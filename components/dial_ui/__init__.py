"""dial_ui: the whole Dialed UI, built in C++ on ESPHome's LVGL instance.

See CLAUDE.md (architecture), spec.md (screens) and contract.md (data contract).
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.lvgl.defines import CONF_LVGL_ID
from esphome.components.lvgl.lvcode import LvglComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["lvgl"]

# design/lvgl holds ui_tokens.h, the single source of truth for colours and sizes.
DESIGN_LVGL_DIR = Path(__file__).resolve().parents[2] / "design" / "lvgl"

dial_ui_ns = cg.esphome_ns.namespace("dial_ui")
DialUi = dial_ui_ns.class_("DialUi", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DialUi),
        cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    if not (DESIGN_LVGL_DIR / "ui_tokens.h").is_file():
        raise cv.Invalid(f"ui_tokens.h not found in {DESIGN_LVGL_DIR}")
    cg.add_build_flag(f"-I{DESIGN_LVGL_DIR}")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_LVGL_ID])
