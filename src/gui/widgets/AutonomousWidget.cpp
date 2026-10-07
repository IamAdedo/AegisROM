// AutonomousWidget implementation.
#include "AutonomousWidget.h"

#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "AegisAgent.h"
#include "FlashromHAL.h"

AutonomousWidget::AutonomousWidget(aegis::FlashromHAL& hal, QWidget* parent)
    : QWidget(parent), hal_(hal) {
  auto* layout = new QVBoxLayout(this);
  targetCombo_ = new QComboBox(this);
  targetCombo_->addItems({"generic_spi_nor", "laptop_bios"});
  goButton_ = new QPushButton("One-Click AI Auto-Flash", this);
  logView_ = new QPlainTextEdit(this);
  logView_->setReadOnly(true);
  layout->addWidget(targetCombo_);
  layout->addWidget(goButton_);
  layout->addWidget(logView_);
  connect(goButton_, &QPushButton::clicked, this,
          &AutonomousWidget::onAutoFlashClicked);
}

void AutonomousWidget::onAutoFlashClicked() {
  logView_->appendPlainText("=== autonomous run started ===");
  aegis::AgentConfig cfg;
  cfg.targetType = targetCombo_->currentText().toStdString();
  aegis::AegisAgent agent(hal_);
  agent.setDecisionLog(
      [this](const std::string& s) { logView_->appendPlainText(QString::fromStdString(s)); });
  if (!agent.runAutoFlash(cfg)) {
    logView_->appendPlainText(
        QString("FAILED: %1").arg(QString::fromStdString(agent.lastError())));
    return;
  }
  logView_->appendPlainText("=== autonomous run complete ===");
}
