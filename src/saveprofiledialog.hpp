#pragma once

#include "igdbclient.hpp"

#include <QDialog>
#include <QPixmap>

class QLabel;
class QLineEdit;
class QPushButton;
class QHBoxLayout;
class QTimer;

class SaveProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit SaveProfileDialog(IgdbClient* igdb,
                               const QString& initial_name = {},
                               QWidget* parent = nullptr);

    QString profile_name()    const;
    QPixmap selected_cover()  const;   // null pixmap if user chose no image

private slots:
    void on_search_triggered();
    void on_results_ready(const QList<IgdbClient::GameResult>& results);
    void on_cover_ready(const QString& image_id, const QPixmap& pixmap);

private:
    void select_cover(int idx);
    void clear_thumbnails();

    IgdbClient*         igdb_;
    QLineEdit*          name_edit_;
    QLabel*             status_label_;
    QWidget*            thumb_container_;
    QHBoxLayout*        thumb_layout_;
    QTimer*             debounce_;
    QList<QPushButton*> thumb_buttons_;
    QList<QPixmap>      covers_;
    QList<QString>      image_ids_;
    int                 selected_idx_ = -1;
};
