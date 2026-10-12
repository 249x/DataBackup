#pragma once

#include <QString>

// 界面外观（QSS）单独一个文件：main 只管装配，不掺样式文本。
// 要调配色、圆角、字号、控件间距，只动这里。
namespace Style {

QString Sheet();

} // namespace Style
