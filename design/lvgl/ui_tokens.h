/* Dialed UI tokens for LVGL 9. Mirrors tokens.json; keep the two in sync. */
#ifndef DIALED_UI_TOKENS_H
#define DIALED_UI_TOKENS_H
#include "lvgl.h"

/* Colours. Define DIALED_THEME_DAY for the day theme. */
#ifndef DIALED_THEME_DAY
#define DIALED_BG         lv_color_hex(0x05080B)
#define DIALED_SURFACE    lv_color_hex(0x12181D)
#define DIALED_SURFACE_2  lv_color_hex(0x1C252C)
#define DIALED_TRACK      lv_color_hex(0x1F2A31)
#define DIALED_LINE       lv_color_hex(0x66788A)
#define DIALED_INK        lv_color_hex(0xF1F5F2)
#define DIALED_INK_MUTED  lv_color_hex(0x9FB0BA)
#define DIALED_ACCENT     lv_color_hex(0x5EF0B0)
#define DIALED_ON_ACCENT  lv_color_hex(0x04231A)
#define DIALED_OK         lv_color_hex(0x4D9FFF)
#define DIALED_WARN       lv_color_hex(0xF2B134)
#define DIALED_DANGER     lv_color_hex(0xFF6A55)
#define DIALED_RAIN_1     lv_color_hex(0xA6D0FF)
#define DIALED_RAIN_2     lv_color_hex(0x4F8FFF)
#define DIALED_RAIN_3     lv_color_hex(0x2A55E8)
#define DIALED_METRIC_TEMP     lv_color_hex(0xFF8A5C)
#define DIALED_METRIC_HUMIDITY lv_color_hex(0x5CC8FF)
#define DIALED_METRIC_AIR      lv_color_hex(0x7BE495)
#else
#define DIALED_BG         lv_color_hex(0xF2F4F1)
#define DIALED_SURFACE    lv_color_hex(0xE2E6E1)
#define DIALED_SURFACE_2  lv_color_hex(0xD2D8D2)
#define DIALED_TRACK      lv_color_hex(0xCDD4CD)
#define DIALED_LINE       lv_color_hex(0x6B776C)
#define DIALED_INK        lv_color_hex(0x12181D)
#define DIALED_INK_MUTED  lv_color_hex(0x4A5650)
#define DIALED_ACCENT     lv_color_hex(0x00734A)
#define DIALED_ON_ACCENT  lv_color_hex(0xFFFFFF)
#define DIALED_OK         lv_color_hex(0x0A5FC4)
#define DIALED_WARN       lv_color_hex(0x8A5A00)
#define DIALED_DANGER     lv_color_hex(0xB3261E)
#define DIALED_RAIN_1     lv_color_hex(0x7FB0EA)
#define DIALED_RAIN_2     lv_color_hex(0x2F72D0)
#define DIALED_RAIN_3     lv_color_hex(0x0D3A96)
#define DIALED_METRIC_TEMP     lv_color_hex(0xC2410C)
#define DIALED_METRIC_HUMIDITY lv_color_hex(0x0A6A9E)
#define DIALED_METRIC_AIR      lv_color_hex(0x1A7F37)
#endif

/* Fonts (enable the matching LV_FONT_MONTSERRAT_x in lv_conf.h). */
#define DIALED_FONT_VALUE_XL (&lv_font_montserrat_48)
#define DIALED_FONT_VALUE_L  (&lv_font_montserrat_40)
#define DIALED_FONT_VALUE_M  (&lv_font_montserrat_28)
#define DIALED_FONT_TITLE    (&lv_font_montserrat_20)
#define DIALED_FONT_BODY     (&lv_font_montserrat_16)
#define DIALED_FONT_LABEL    (&lv_font_montserrat_14)
#define DIALED_FONT_CAPTION  (&lv_font_montserrat_12)

/* Geometry, px. */
#define DIALED_DISPLAY      240
#define DIALED_RING_W       10
#define DIALED_RING_INSET   4
#define DIALED_ARC_SIZE     (DIALED_DISPLAY - 2 * DIALED_RING_INSET)   /* 232 */
#define DIALED_ARC_ROTATION 135
#define DIALED_ARC_SWEEP    270
#define DIALED_CONTENT_R    98
#define DIALED_SAFE_BOX     160
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_ICON_S       16
#define DIALED_ICON_M       32
#define DIALED_ICON_L       48
#define DIALED_TOUCH_MIN    40
#define DIALED_ROW_H        44

#define DIALED_SPACE_1 4
#define DIALED_SPACE_2 8
#define DIALED_SPACE_3 12
#define DIALED_SPACE_4 16
#define DIALED_SPACE_5 24
#define DIALED_SPACE_6 32

#define DIALED_RADIUS_SM   6
#define DIALED_RADIUS_MD   12

#define DIALED_OPA_PRESSED  LV_OPA_70
#define DIALED_OPA_DIM      LV_OPA_60
#define DIALED_OPA_DISABLED LV_OPA_40

#endif /* DIALED_UI_TOKENS_H */
