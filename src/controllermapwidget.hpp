#pragma once

#include "mapping.hpp"

#include <QPixmap>
#include <QSet>
#include <QWidget>

// Draws a DualSense silhouette with callout lines showing keyboard→button mappings.
class ControllerMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit ControllerMapWidget(QWidget* parent = nullptr);

    void set_mappings(const QList<kb::Mapping>& mappings);
    void set_mouse_stick(const kb::MouseStickConfig& ms);
    void set_active_keys(QSet<int> codes);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct AnchorPoint {
        const char* output_name;   // matches DS5 output display name
        float label_nx, label_ny;  // normalised coords where the text box is placed
    };

    static const AnchorPoint ANCHORS[];
    static int ANCHOR_COUNT;

    // Returns the short key/input label for a mapping (e.g. "W", "Space", "Mouse L")
    static QString input_label(const kb::Mapping& m);

    // Finds the DS5 output name for a mapping by matching its fields to DS5_OUTPUTS
    static QString output_name(const kb::Mapping& m);

    // Returns anchor for a given DS5 output name, or nullptr
    const AnchorPoint* find_anchor(const QString& name) const;

    QList<kb::Mapping>   mappings_;
    kb::MouseStickConfig mouse_stick_;
    QPixmap              controller_img_;
    QSet<int>            active_keys_;
    QPointF              mouse_pos_       = {-1, -1};  // widget coords; (-1,-1) = outside
};
