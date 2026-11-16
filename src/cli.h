#pragma once

#include <QStringList>

namespace cli {

/// Command line interface (`claudemods-hello fix|tweak|dns|launch ...`).
/// `args` is QCoreApplication::arguments() (program name first). Returns the exit code.
int run(const QStringList &args);

} // namespace cli
