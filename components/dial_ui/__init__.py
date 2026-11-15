"""dial_ui: the whole Dialed UI, built in C++ on ESPHome's LVGL instance.

See CLAUDE.md (architecture), spec.md (screens) and contract.md (data contract).
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.lvgl.defines import CONF_LVGL_ID
from esphome.components.lvgl.lvcode import LvglComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.helpers import copy_file_if_changed

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
