// aegisrom — Qt6 GUI entry point.
#include <QApplication>

#include "FlashromHAL.h"
#include "MainWindow.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  aegis::FlashromHAL hal;
  hal.init(false);
  MainWindow win(hal);
  win.show();
  return app.exec();
}
