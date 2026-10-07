#pragma once
// AutonomousWidget — "One-Click AI Auto-Flash" panel (GUI agentic mode).
//
// Streams AegisAgent decision callbacks into the log view while the agent
// runs probe -> header -> voltage -> backup -> erase -> write -> verify.
// Phase 3 moves execution onto a worker thread; this revision runs inline.
#include <QWidget>

namespace aegis {
class AegisAgent;
class FlashromHAL;
}  // namespace aegis

class QComboBox;
class QPlainTextEdit;
class QPushButton;

class AutonomousWidget : public QWidget {
  Q_OBJECT
 public:
  AutonomousWidget(aegis::FlashromHAL& hal, QWidget* parent = nullptr);

 private slots:
  void onAutoFlashClicked();

 private:
  aegis::FlashromHAL& hal_;
  QComboBox* targetCombo_ = nullptr;
  QPushButton* goButton_ = nullptr;
  QPlainTextEdit* logView_ = nullptr;
};
