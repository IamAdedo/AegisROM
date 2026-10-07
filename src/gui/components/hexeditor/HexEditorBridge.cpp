// HexEditorBridge implementation (placeholder view until qhexedit2 port).
#include "HexEditorBridge.h"

#include <algorithm>

#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QString>
#include <QVBoxLayout>

HexEditorBridge::HexEditorBridge(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  view_ = new QPlainTextEdit(this);
  view_->setReadOnly(false);
  view_->setPlaceholderText("Flash image hex dump (qhexedit2 port lands in Phase 2)");
  view_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  layout->addWidget(view_);
}

void HexEditorBridge::setData(const std::vector<std::uint8_t>& data) {
  data_ = data;
  refreshView();
  emit dataChanged();
}

std::vector<std::uint8_t> HexEditorBridge::data() const { return data_; }

void HexEditorBridge::clear() {
  data_.clear();
  refreshView();
  emit dataChanged();
}

void HexEditorBridge::refreshView() {
  static constexpr std::size_t kPreviewMax = 4096;
  QString text;
  text.reserve(8192);
  const std::size_t n = std::min(data_.size(), kPreviewMax);
  for (std::size_t i = 0; i < n; i += 16) {
    text += QString("%1  ").arg(static_cast<uint>(i), 8, 16, QChar('0'));
    for (std::size_t j = i; j < std::min(i + 16, n); ++j) {
      text += QString("%1 ").arg(data_[j], 2, 16, QChar('0'));
    }
    text += '\n';
  }
  if (data_.size() > kPreviewMax) {
    text += QString("... (%1 more bytes, full buffer held in memory)\n")
                .arg(data_.size() - kPreviewMax);
  }
  view_->setPlainText(text);
}
