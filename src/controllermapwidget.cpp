#include "controllermapwidget.hpp"
#include "mappingstorage.hpp"
#include "apptheme.hpp"

#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>

// ── Anchor table ──────────────────────────────────────────────────────────────
// label_nx/label_ny: normalised position where the text box is placed (0..1).

const ControllerMapWidget::AnchorPoint ControllerMapWidget::ANCHORS[] = {
    // name                   label_nx  label_ny
    {"L2 (digital)",           0.050f,  0.100f},
    {"L2 (analog)",            0.050f,  0.100f},
    {"L1",                     0.050f,  0.250f},
    {"R2 (digital)",           0.925f,  0.120f},
    {"R2 (analog)",            0.925f,  0.120f},
    {"R1",                     0.925f,  0.250f},

    {"DPad Up",                0.070f,  0.580f},
    {"DPad Down",              0.070f,  0.420f},
    {"DPad Left",              0.070f,  0.660f},
    {"DPad Right",             0.070f,  0.500f},

    {"Left Stick Left",        0.150f,  0.940f},
    {"Left Stick Right",       0.150f,  0.940f},
    {"Left Stick Up",          0.150f,  0.940f},
    {"Left Stick Down",        0.150f,  0.940f},
    {"L3",                     0.090f,  0.940f},

    {"Create",                 0.340f,  0.430f},
    {"Touchpad",               0.440f,  0.065f},
    {"PS",                     0.500f,  0.885f},
    {"Mute",                   0.500f,  0.610f},
    {"Options",                0.630f,  0.060f},

    {"Square",                 0.950f,  0.700f},
    {"Triangle",               0.950f,  0.400f},
    {"Circle",                 0.950f,  0.500f},
    {"Cross",                  0.950f,  0.600f},

    {"Right Stick Left",       0.850f,  0.940f},
    {"Right Stick Right",      0.850f,  0.940f},
    {"Right Stick Up",         0.850f,  0.940f},
    {"Right Stick Down",       0.850f,  0.940f},
    {"R3",                     0.910f,  0.940f},
};
int ControllerMapWidget::ANCHOR_COUNT =
    static_cast<int>(sizeof(ANCHORS) / sizeof(ANCHORS[0]));

// ── Constructor ───────────────────────────────────────────────────────────────

ControllerMapWidget::ControllerMapWidget(QWidget* parent)
    : QWidget(parent)
    , controller_img_(":/controller.png")
{
    setMinimumHeight(320);
    setMouseTracking(true);
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

void ControllerMapWidget::set_active_keys(QSet<int> codes)
{
    active_keys_ = std::move(codes);
    update();
}

void ControllerMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    mouse_pos_ = event->position();
    update();
}

void ControllerMapWidget::leaveEvent(QEvent*)
{
    mouse_pos_ = {-1, -1};
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
    constexpr float CTRL_H = 243.0f;   // 400/243 ≈ 1.646, matches controller.png (1200×729)
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

    QSet<QString> active_anchors;
    for (const auto& m : mappings_) {
        if (!m.enabled) continue;
        const QString oname = output_name(m);
        if (oname.isEmpty()) continue;
        if (!find_anchor(oname)) continue;
        anchor_keys[oname].append(input_label(m));
        if (m.input_kind == kb::InputKind::Key && active_keys_.contains(m.input_code))
            active_anchors.insert(oname);
    }
    // Mouse stick
    if (mouse_stick_.enabled) {
        const char* stick_name = mouse_stick_.use_right_stick ? "Right Stick Up" : "Left Stick Up";
        anchor_keys[QString::fromLatin1(stick_name)].append("Mouse");
    }

    const float label_font_sz = qMax(8.0f, 7.0f * scale);
    QFont label_font;
    label_font.setPixelSize(static_cast<int>(label_font_sz));
    label_font.setBold(true);
    p.setFont(label_font);
    QFontMetrics fm(label_font);

    const float PAD       = 4.0f  * scale;
    const QColor LINE_COL(0x00, 0xc9, 0xa7, 180);
    const QColor BOX_COL (0x1a, 0x1a, 0x30, 220);

    for (auto it = anchor_keys.constBegin(); it != anchor_keys.constEnd(); ++it) {
        const AnchorPoint* a = find_anchor(it.key());
        if (!a) continue;

        const QPointF label_pt = ctrl_pt(a->label_nx, a->label_ny);

        // Stack all key names for this anchor
        const QStringList& keys = it.value();
        const QString text = keys.join(" / ");

        const QRectF text_r = fm.boundingRect(text);
        const float tw = text_r.width()  + 2 * PAD;
        const float th = text_r.height() + 2 * PAD;

        // Centre the box on label_pt
        const QRectF box(label_pt.x() - tw / 2.0f, label_pt.y() - th / 2.0f, tw, th);

        const bool    active    = active_anchors.contains(it.key());
        const QColor  accent    = active ? QColor(Themes::current().accent) : QColor();
        const QColor  fill      = active ? accent                           : BOX_COL;
        const QColor  border    = active ? accent.lighter(130)              : LINE_COL.darker(120);
        const QColor  text_col  = active ? QColor(Themes::current().onaccent)
                                         : QColor(0xe0, 0xe0, 0xf0);

        p.setPen(QPen(border, 1.0 * scale));
        p.setBrush(fill);
        p.drawRoundedRect(box, 3 * scale, 3 * scale);

        p.setPen(text_col);
        p.drawText(box, Qt::AlignCenter, text);
    }

    // ── Debug: nx/ny readout ──────────────────────────────────────────────────
    if (mouse_pos_.x() >= 0) {
        const float nx = static_cast<float>((mouse_pos_.x() - ox) / cw);
        const float ny = static_cast<float>((mouse_pos_.y() - oy) / ch);
        const QString coord = QString("nx %1  ny %2")
            .arg(static_cast<double>(nx), 0, 'f', 3)
            .arg(static_cast<double>(ny), 0, 'f', 3);

        QFont dbg_font;
        dbg_font.setPixelSize(11);
        dbg_font.setBold(true);
        p.setFont(dbg_font);
        QFontMetrics dbg_fm(dbg_font);
        const QRectF dbg_tr = dbg_fm.boundingRect(coord);
        const float  dbg_pad = 5.0f;
        const QRectF dbg_box(8, 8, dbg_tr.width() + 2 * dbg_pad, dbg_tr.height() + 2 * dbg_pad);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x00, 0x00, 0x00, 180));
        p.drawRoundedRect(dbg_box, 4, 4);
        p.setPen(QColor(0x00, 0xff, 0xcc));
        p.drawText(dbg_box, Qt::AlignCenter, coord);
    }
}
