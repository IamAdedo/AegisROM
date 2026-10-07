#pragma once
// MainWindow — AegisROM Qt6 shell: manual tab (hex editor + pinout +
// read/write/erase) and agentic tab (AutonomousWidget).
#include <QMainWindow>

namespace aegis {
class FlashromHAL;
}  // namespace aegis

class QComboBox;
class QPlainTextEdit;
class AutonomousWidget;
class HexEditorBridge;
class PinoutViewer;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(aegis::FlashromHAL& hal, QWidget* parent = nullptr);

 private slots:
  void onRead();
  void onWrite();
  void onErase();
  void onVerify();

 private:
  void log(const QString& msg);

  aegis::FlashromHAL& hal_;
  QComboBox* programmerCombo_ = nullptr;
  QComboBox* chipCombo_ = nullptr;
  QPlainTextEdit* logView_ = nullptr;
  HexEditorBridge* hexEditor_ = nullptr;
  PinoutViewer* pinout_ = nullptr;
  AutonomousWidget* autonomous_ = nullptr;
};
