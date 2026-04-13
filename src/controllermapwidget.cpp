#include "controllermapwidget.hpp"
#include "mappingstorage.hpp"

#include <QPainter>
#include <QFontMetrics>

// ── Anchor table ──────────────────────────────────────────────────────────────
// Coordinates are normalised to a 400×260 controller bounding box.
// label_dir_x/y are the direction the callout line extends from the button.

const ControllerMapWidget::AnchorPoint ControllerMapWidget::ANCHORS[] = {
    // name                   nx       ny     lx     ly
    {"L2 (digital)",        0.210f,  0.070f, -1.0f, -0.8f},
    {"L2 (analog)",         0.210f,  0.070f, -1.0f, -0.8f},
    {"L1",                  0.170f,  0.165f, -1.0f, -0.3f},
    {"R2 (digital)",        0.790f,  0.050f,  1.0f, -0.8f},
    {"R2 (analog)",         0.790f,  0.050f,  1.0f, -0.8f},
    {"R1",                  0.840f,  0.165f,  1.0f, -0.3f},

    {"DPad Up",             0.210f,  0.400f, -1.0f, -1.0f},
    {"DPad Down",           0.210f,  0.520f, -1.0f,  1.0f},
    {"DPad Left",           0.140f,  0.480f, -1.0f,  0.0f},
    {"DPad Right",          0.260f,  0.480f, -0.25f, -1.0f},

    {"Left Stick Left",     0.330f,  0.630f, -1.0f,  0.3f},
    {"Left Stick Right",    0.330f,  0.630f, -1.0f,  0.3f},
    {"Left Stick Up",       0.330f,  0.630f, -1.0f,  0.3f},
    {"Left Stick Down",     0.330f,  0.630f, -1.0f,  0.3f},
    {"L3",                  0.330f,  0.730f, -1.0f,  0.6f},

    {"Create",              0.280f,  0.350f, -0.5f, -1.0f},
    {"Touchpad",            0.500f,  0.350f,  0.0f, -1.0f},
    {"PS",                  0.500f,  0.560f,  0.0f,  1.0f},
    {"Mute",                0.500f,  0.610f,  0.0f,  1.0f},
    {"Options",             0.720f,  0.350f,  0.5f, -1.0f},

    {"Square",              0.725f,  0.470f,  0.7f, -1.0f},
    {"Triangle",            0.785f,  0.385f,  1.0f, -1.0f},
    {"Circle",              0.845f,  0.470f,  1.0f,  0.0f},
    {"Cross",               0.785f,  0.560f,  1.0f,  1.0f},

    {"Right Stick Left",    0.640f,  0.630f,  1.0f,  0.3f},
    {"Right Stick Right",   0.640f,  0.630f,  1.0f,  0.3f},
    {"Right Stick Up",      0.640f,  0.630f,  1.0f,  0.3f},
    {"Right Stick Down",    0.640f,  0.630f,  1.0f,  0.3f},
    {"R3",                  0.640f,  0.730f,  1.0f,  0.6f},
};
int ControllerMapWidget::ANCHOR_COUNT =
    static_cast<int>(sizeof(ANCHORS) / sizeof(ANCHORS[0]));

// ── Constructor ───────────────────────────────────────────────────────────────

ControllerMapWidget::ControllerMapWidget(QWidget* parent)
    : QWidget(parent)
    , controller_img_(":/ps5_diagram_simple.png")
{
    setMinimumHeight(320);
}

void ControllerMapWidget::set_mappings(const QList<kb::Mapping>& mappings)
{
    mappings_ = mappings;
    update();
}

void ControllerMapWidget::set_mouse_stick(const kb::MouseStickConfig& ms)
{
    mouse_stick_ = ms;
    update();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ControllerMapWidget::input_label(const kb::Mapping& m)
{
    if (m.input_kind == kb::InputKind::MouseAxis)
        return "Mouse";

    // Use the mapping label's left side if it follows "X → Y" format
    const int arrow = m.label.indexOf(" \u2192 ");
    if (arrow > 0)
        return m.label.left(arrow);

    return MappingStorage::keyName(m.input_code);
}

QString ControllerMapWidget::output_name(const kb::Mapping& m)
{
    for (int i = 0; i < kb::DS5_OUTPUT_COUNT; ++i) {
        const auto& o = kb::DS5_OUTPUTS[i];
        if (o.kind != m.output_kind) continue;
        switch (m.output_kind) {
        case kb::OutputKind::Button:
            if (o.btn_byte == m.btn_byte && o.btn_mask == m.btn_mask)
                return QString::fromLatin1(o.name);
            break;
        case kb::OutputKind::DpadDir:
            if (o.dpad_bit == m.dpad_bit)
                return QString::fromLatin1(o.name);
            break;
        case kb::OutputKind::AxisFixed:
            if (o.axis_offset == m.axis_offset && o.axis_value == m.axis_value)
                return QString::fromLatin1(o.name);
            break;
        }
    }
    return {};
}

const ControllerMapWidget::AnchorPoint*
ControllerMapWidget::find_anchor(const QString& name) const
{
    for (int i = 0; i < ANCHOR_COUNT; ++i) {
        if (name == QLatin1String(ANCHORS[i].output_name))
            return &ANCHORS[i];
    }
    return nullptr;
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void ControllerMapWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ── Background ────────────────────────────────────────────────────────────
    p.fillRect(rect(), QColor(0x12, 0x12, 0x1f));

    // ── Controller bounding box (centred, with margin for labels) ─────────────
    constexpr float CTRL_W = 400.0f;
    constexpr float CTRL_H = 260.0f;
    constexpr float MARGIN = 80.0f;   // space around controller for callout labels

    const float avail_w = width()  - 2 * MARGIN;
    const float avail_h = height() - 2 * MARGIN;
    const float scale   = qMin(avail_w / CTRL_W, avail_h / CTRL_H);
    const float cw      = CTRL_W * scale;
    const float ch      = CTRL_H * scale;
    const float ox      = (width()  - cw) / 2.0f;
    const float oy      = (height() - ch) / 2.0f;

    auto ctrl_pt = [&](float nx, float ny) -> QPointF {
        return {ox + nx * cw, oy + ny * ch};
    };

    // ── Draw controller image ─────────────────────────────────────────────────
    if (!controller_img_.isNull())
        p.drawPixmap(QRectF(ox, oy, cw, ch), controller_img_, QRectF(controller_img_.rect()));

    // ── Callout lines + labels for each active mapping ─────────────────────────
    // Gather labels per anchor to stack them if multiple keys share one button
    struct LabelEntry { QString key_text; const AnchorPoint* anchor; };
    QMap<QString, QStringList> anchor_keys; // output_name -> [key labels]

    for (const auto& m : mappings_) {
        if (!m.enabled) continue;
        const QString oname = output_name(m);
        if (oname.isEmpty()) continue;
        if (!find_anchor(oname)) continue;
        anchor_keys[oname].append(input_label(m));
    }
    // Mouse stick
    if (mouse_stick_.enabled) {
        const char* stick_name = mouse_stick_.use_right_stick ? "Right Stick Up" : "Left Stick Up";
        anchor_keys[QString::fromLatin1(stick_name)].append("Mouse");
    }

    const float label_font_sz = qMax(8.0f, 9.0f * scale);
    QFont label_font;
    label_font.setPixelSize(static_cast<int>(label_font_sz));
    label_font.setBold(true);
    p.setFont(label_font);
    QFontMetrics fm(label_font);

    const float LINE_LEN  = 34.0f * scale;
    const float PAD       = 4.0f  * scale;
    const QColor LINE_COL(0x00, 0xc9, 0xa7, 180);
    const QColor BOX_COL (0x1a, 0x1a, 0x30, 220);

    for (auto it = anchor_keys.constBegin(); it != anchor_keys.constEnd(); ++it) {
        const AnchorPoint* a = find_anchor(it.key());
        if (!a) continue;

        const QPointF btn_pt = ctrl_pt(a->nx, a->ny);
        const float mag = std::hypot(a->label_dir_x, a->label_dir_y);
        const float dx  = a->label_dir_x / mag;
        const float dy  = a->label_dir_y / mag;
        const QPointF line_end(btn_pt.x() + dx * LINE_LEN,
                               btn_pt.y() + dy * LINE_LEN);

        // Draw connector line
        p.setPen(QPen(LINE_COL, 1.2 * scale));
        p.setBrush(Qt::NoBrush);
        p.drawLine(btn_pt, line_end);

        // Stack all key names for this anchor
        const QStringList& keys = it.value();
        const QString text = keys.join(" / ");

        const QRectF text_r = fm.boundingRect(text);
        const float tw = text_r.width()  + 2 * PAD;
        const float th = text_r.height() + 2 * PAD;

        // Position label box so it extends away from line_end
        float bx = line_end.x();
        float by = line_end.y();
        if (dx >= 0) bx = line_end.x();
        else         bx = line_end.x() - tw;
        if (dy >= 0) by = line_end.y();
        else         by = line_end.y() - th;

        const QRectF box(bx, by, tw, th);

        p.setPen(QPen(LINE_COL.darker(120), 1.0 * scale));
        p.setBrush(BOX_COL);
        p.drawRoundedRect(box, 3 * scale, 3 * scale);

        p.setPen(QColor(0xe0, 0xe0, 0xf0));
        p.drawText(box, Qt::AlignCenter, text);

        // Small dot at button site
        p.setPen(Qt::NoPen);
        p.setBrush(LINE_COL);
        p.drawEllipse(btn_pt, 2.5 * scale, 2.5 * scale);
    }
}
