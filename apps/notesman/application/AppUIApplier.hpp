#pragma once

#include "gui/UiConstants.hpp"

#include <QTranslator>
#include <memory>

// Free functions tác động lên qApp — không phụ thuộc MainWindow hay AppController.
// applyLanguage nhận translator qua reference để caller kiểm soát lifetime:
//   - AppController: m_translator (member, alive suốt vòng đời controller)
//   - PackerLauncher: local unique_ptr cùng scope với dialog exec()
namespace AppUI {

void applyTheme(UiConst::Theme theme);
void applyLanguage(UiConst::Language lang, std::unique_ptr<QTranslator>& translator);

} // namespace AppUI