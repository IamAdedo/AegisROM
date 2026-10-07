#pragma once
// HexEditorBridge — Phase 2 integration point for IMSProg's qhexedit2.
//
// IMSProg ships qhexedit2 under IMSProg_programmer/qhexedit2/src
// (qhexedit.h/.cpp, chunks.h, commands.h, color_manager.h). The port target is
// src/gui/components/hexeditor/qhexedit2/ with live buffer sync into
// FlashromHAL. Until that port lands, this bridge offers the same data API
// over a plain text hex dump so the GUI builds and manual flows work.
#include <cstddef>
#include <cstdint>
#include <vector>

#include <QWidget>

class QPlainTextEdit;

class HexEditorBridge : public QWidget {
  Q_OBJECT
 public:
  explicit HexEditorBridge(QWidget* parent = nullptr);

  void setData(const std::vector<std::uint8_t>& data);
  [[nodiscard]] std::vector<std::uint8_t> data() const;
  [[nodiscard]] std::size_t size() const { return data_.size(); }
  void clear();

 signals:
  void dataChanged();

 private:
  void refreshView();

  QPlainTextEdit* view_ = nullptr;
  std::vector<std::uint8_t> data_;
};
