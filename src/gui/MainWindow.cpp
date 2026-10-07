// MainWindow implementation — manual + agentic tabs over FlashromHAL.
#include "MainWindow.h"

#include <cstdint>
#include <vector>

#include <QByteArray>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include "AutonomousWidget.h"
#include "FlashromHAL.h"
#include "HashUtils.h"
#include "HexEditorBridge.h"
#include "PinoutViewer.h"

MainWindow::MainWindow(aegis::FlashromHAL& hal, QWidget* parent)
    : QMainWindow(parent), hal_(hal) {
  setWindowTitle("AegisROM");
  resize(1000, 700);

  auto* tabs = new QTabWidget(this);
  setCentralWidget(tabs);

  // ---- Manual tab ----
  auto* manual = new QWidget(this);
  auto* manualLayout = new QVBoxLayout(manual);

  auto* topRow = new QHBoxLayout();
  programmerCombo_ = new QComboBox(manual);
  chipCombo_ = new QComboBox(manual);
  chipCombo_->setEditable(true);
  for (const auto& p : hal_.listProgrammers()) {
    programmerCombo_->addItem(QString::fromStdString(p));
  }
  topRow->addWidget(programmerCombo_);
  topRow->addWidget(chipCombo_);
  manualLayout->addLayout(topRow);

  auto* splitter = new QSplitter(manual);
  hexEditor_ = new HexEditorBridge(splitter);
  pinout_ = new PinoutViewer(splitter);
  splitter->addWidget(hexEditor_);
  splitter->addWidget(pinout_);
  manualLayout->addWidget(splitter, 1);

  auto* btnRow = new QHBoxLayout();
  auto* readBtn = new QPushButton("Read", manual);
  auto* writeBtn = new QPushButton("Write", manual);
  auto* eraseBtn = new QPushButton("Erase", manual);
  auto* verifyBtn = new QPushButton("Verify", manual);
  btnRow->addWidget(readBtn);
  btnRow->addWidget(writeBtn);
  btnRow->addWidget(eraseBtn);
  btnRow->addWidget(verifyBtn);
  manualLayout->addLayout(btnRow);

  logView_ = new QPlainTextEdit(manual);
  logView_->setReadOnly(true);
  logView_->setMaximumBlockCount(1000);
  manualLayout->addWidget(logView_);

  connect(readBtn, &QPushButton::clicked, this, &MainWindow::onRead);
  connect(writeBtn, &QPushButton::clicked, this, &MainWindow::onWrite);
  connect(eraseBtn, &QPushButton::clicked, this, &MainWindow::onErase);
  connect(verifyBtn, &QPushButton::clicked, this, &MainWindow::onVerify);

  tabs->addTab(manual, "Manual");

  // ---- Agentic tab ----
  autonomous_ = new AutonomousWidget(hal_, this);
  tabs->addTab(autonomous_, "Autonomous");

  hal_.setLogCallback([this](int level, const std::string& msg) {
    log(QString("[flashrom:%1] %2").arg(level).arg(QString::fromStdString(msg)));
  });

  statusBar()->showMessage(hal_.isMock() ? "Mock HAL (no hardware)" : "HAL ready");
  log(QString("AegisROM ready — %1").arg(QString::fromUtf8(aegis::FlashromHAL::version())));
}

void MainWindow::log(const QString& msg) {
  logView_->appendPlainText(msg);
  statusBar()->showMessage(msg.left(120));
}

namespace {
bool ensureOpen(aegis::FlashromHAL& hal, const QString& programmer, const QString& chip,
                QString& err) {
  if (!hal.isOpen() &&
      !hal.open(programmer.toStdString(), "", chip.toStdString())) {
    err = QString::fromStdString(hal.lastError());
    return false;
  }
  return true;
}
}  // namespace

void MainWindow::onRead() {
  QString err;
  if (!ensureOpen(hal_, programmerCombo_->currentText(), chipCombo_->currentText(), err)) {
    QMessageBox::warning(this, "Open failed", err);
    return;
  }
  std::vector<std::uint8_t> data;
  if (!hal_.read(data)) {
    QMessageBox::warning(this, "Read failed", QString::fromStdString(hal_.lastError()));
    return;
  }
  hexEditor_->setData(data);
  log(QString("read %1 bytes sha256=%2")
          .arg(data.size())
          .arg(QString::fromStdString(aegis::hash::toHex(aegis::hash::sha256(data)))));
}

void MainWindow::onWrite() {
  const QString path = QFileDialog::getOpenFileName(this, "Select image to write");
  if (path.isEmpty()) return;
  QString err;
  if (!ensureOpen(hal_, programmerCombo_->currentText(), chipCombo_->currentText(), err)) {
    QMessageBox::warning(this, "Open failed", err);
    return;
  }
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Write failed", "cannot open image file");
    return;
  }
  const QByteArray blob = f.readAll();
  std::vector<std::uint8_t> image(blob.begin(), blob.end());
  std::vector<std::uint8_t> backup;
  if (!hal_.read(backup)) {
    QMessageBox::warning(this, "Write blocked", "pre-write backup failed");
    return;
  }
  if (!hal_.erase() || !hal_.write(image) || !hal_.verify(image)) {
    QMessageBox::warning(this, "Write failed",
                         QString::fromStdString(hal_.lastError()));
    return;
  }
  hexEditor_->setData(image);
  log("write+verify OK");
}

void MainWindow::onErase() {
  QString err;
  if (!ensureOpen(hal_, programmerCombo_->currentText(), chipCombo_->currentText(), err)) {
    QMessageBox::warning(this, "Open failed", err);
    return;
  }
  std::vector<std::uint8_t> backup;
  if (!hal_.read(backup)) {
    QMessageBox::warning(this, "Erase blocked", "pre-erase backup failed");
    return;
  }
  if (!hal_.erase()) {
    QMessageBox::warning(this, "Erase failed", QString::fromStdString(hal_.lastError()));
    return;
  }
  log("erase complete");
}

void MainWindow::onVerify() {
  const QString path = QFileDialog::getOpenFileName(this, "Select image to verify against");
  if (path.isEmpty()) return;
  QString err;
  if (!ensureOpen(hal_, programmerCombo_->currentText(), chipCombo_->currentText(), err)) {
    QMessageBox::warning(this, "Open failed", err);
    return;
  }
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Verify failed", "cannot open image file");
    return;
  }
  const QByteArray blob = f.readAll();
  const std::vector<std::uint8_t> image(blob.begin(), blob.end());
  if (hal_.verify(image)) {
    log("verify OK");
  } else {
    QMessageBox::warning(this, "Verify", "MISMATCH");
  }
}
