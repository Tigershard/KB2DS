#pragma once

#include "mapping.hpp"

#include <QPixmap>
#include <QWidget>

// Draws a DualSense silhouette with callout lines showing keyboard→button mappings.
class ControllerMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit ControllerMapWidget(QWidget* parent = nullptr);

    void set_mappings(const QList<kb::Mapping>& mappings);
    void set_mouse_stick(const kb::MouseStickConfig& ms);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct AnchorPoint {
        const char* output_name;   // matches DS5 output display name
        float nx, ny;              // normalised coords in controller space (0..1)
        float label_dir_x;         // unit-ish vector for label placement
        float label_dir_y;
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
};
