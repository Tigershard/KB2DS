#include "saveprofiledialog.hpp"
#include "apptheme.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

SaveProfileDialog::SaveProfileDialog(IgdbClient* igdb,
                                     const QString& initial_name,
                                     QWidget* parent)
    : QDialog(parent)
    , igdb_(igdb)
{
    setWindowTitle("Save Profile");
    setModal(true);
    setMinimumWidth(460);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 16);

    // Name row
    auto* name_row = new QHBoxLayout();
    name_row->addWidget(new QLabel("Profile name:", this));
    name_edit_ = new QLineEdit(initial_name, this);
    name_row->addWidget(name_edit_, 1);
    layout->addLayout(name_row);

    // Status / hint label
    const bool can_search = igdb_ && igdb_->has_credentials();
    status_label_ = new QLabel(
        can_search ? "Type a name to search for game cover art (optional)."
                   : "Add IGDB credentials in Settings to enable cover art search.",
        this);
    status_label_->setStyleSheet(
        QString("color: %1; font-size: 11px;").arg(Themes::current().textm));
    layout->addWidget(status_label_);

    // Thumbnail strip
    auto* scroll = new QScrollArea(this);
    scroll->setFixedHeight(116);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    thumb_container_ = new QWidget();
    thumb_layout_ = new QHBoxLayout(thumb_container_);
    thumb_layout_->setContentsMargins(6, 4, 6, 4);
    thumb_layout_->setSpacing(8);
    thumb_layout_->addStretch();
    scroll->setWidget(thumb_container_);
    layout->addWidget(scroll);

    // Save / Cancel
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Save");
    buttons->button(QDialogButtonBox::Ok)->setObjectName("OkBtn");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("DangerBtn");
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Debounced search: fires 400 ms after the user stops typing
    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(400);
    connect(debounce_, &QTimer::timeout, this, &SaveProfileDialog::on_search_triggered);

    connect(name_edit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (igdb_ && igdb_->has_credentials() && text.trimmed().length() >= 2)
            debounce_->start();
        else
            debounce_->stop();
    });

    if (igdb_) {
        connect(igdb_, &IgdbClient::results_ready, this, &SaveProfileDialog::on_results_ready);
        connect(igdb_, &IgdbClient::cover_ready,   this, &SaveProfileDialog::on_cover_ready);
        connect(igdb_, &IgdbClient::error, this, [this](const QString& msg) {
            status_label_->setText("Error: " + msg);
            status_label_->setStyleSheet(
                QString("color: %1; font-size: 11px;").arg(Themes::current().danger));
        });
    }

    // Auto-search if an initial name was supplied
    if (can_search && initial_name.trimmed().length() >= 2)
        debounce_->start();
}

QString SaveProfileDialog::profile_name() const
{
    return name_edit_->text().trimmed();
}

QPixmap SaveProfileDialog::selected_cover() const
{
    return covers_.value(selected_idx_);
}

void SaveProfileDialog::on_search_triggered()
{
    const QString q = name_edit_->text().trimmed();
    if (q.isEmpty() || !igdb_) return;
    clear_thumbnails();
    status_label_->setText("Searching...");
    status_label_->setStyleSheet(
        QString("color: %1; font-size: 11px;").arg(Themes::current().textm));
    igdb_->search(q);
}

void SaveProfileDialog::on_results_ready(const QList<IgdbClient::GameResult>& results)
{
    clear_thumbnails();

    if (results.isEmpty()) {
        status_label_->setText("No game art found.");
        return;
    }

    status_label_->setText("Click a cover to attach it to the profile (optional):");
    covers_.resize(results.size());

    for (int i = 0; i < results.size(); ++i) {
        image_ids_ << results[i].image_id;

        auto* btn = new QPushButton(thumb_container_);
        btn->setFixedSize(66, 94);
        btn->setToolTip(results[i].name);
        {
            const AppTheme& t = Themes::current();
            btn->setStyleSheet(QString(
                "QPushButton { background: %1; border: 2px solid %2; border-radius: 4px; }"
                "QPushButton:hover { border-color: %3; }")
                .arg(t.bg3, t.border, t.textm));
        }
        // Insert before the trailing stretch
        thumb_layout_->insertWidget(thumb_layout_->count() - 1, btn);
        thumb_buttons_.append(btn);

        const int idx = i;
        connect(btn, &QPushButton::clicked, this, [this, idx]() { select_cover(idx); });

        igdb_->fetch_cover(results[i].image_id);
    }
}

void SaveProfileDialog::on_cover_ready(const QString& image_id, const QPixmap& pixmap)
{
    const int idx = image_ids_.indexOf(image_id);
    if (idx < 0 || idx >= thumb_buttons_.size()) return;

    covers_[idx] = pixmap;
    thumb_buttons_[idx]->setIcon(QIcon(pixmap));
    thumb_buttons_[idx]->setIconSize({62, 90});
}

void SaveProfileDialog::select_cover(int idx)
{
    selected_idx_ = idx;

    const AppTheme& t = Themes::current();
    for (int i = 0; i < thumb_buttons_.size(); ++i) {
        const bool sel = (i == idx);
        if (sel) {
            thumb_buttons_[i]->setStyleSheet(QString(
                "QPushButton { background: %1; border: 2px solid %2; border-radius: 4px; }")
                .arg(t.bg3, t.accent));
        } else {
            thumb_buttons_[i]->setStyleSheet(QString(
                "QPushButton { background: %1; border: 2px solid %2; border-radius: 4px; }"
                "QPushButton:hover { border-color: %3; }")
                .arg(t.bg3, t.border, t.textm));
        }
    }
}

void SaveProfileDialog::clear_thumbnails()
{
    selected_idx_ = -1;
    image_ids_.clear();
    covers_.clear();
    for (auto* btn : thumb_buttons_) btn->deleteLater();
    thumb_buttons_.clear();
}
