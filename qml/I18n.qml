pragma Singleton
import QtQuick
import ClaudeMods.Hello

// Translation helper. Reading Backend.locale inside the function makes every binding that
// calls I18n.t(...) re-evaluate when the user switches language.
QtObject {
    function t(id, args) {
        const dependsOnLocale = Backend.locale
        return Backend.translate(id, args === undefined ? {} : args)
    }
}
