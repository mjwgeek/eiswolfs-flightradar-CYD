#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: OTA verifies the next boot partition and shows install failures instead of silently restarting into old firmware. The System menu now shows the installed version.";

    const char* const CHANGELOG_DE =
        "- Fix: OTA prüft die Boot-Partition und zeigt Installationsfehler an, statt heimlich die alte Firmware zu starten. Das Systemmenü zeigt die installierte Version.";

    const char* const CHANGELOG_FR =
        "- Correction : OTA vérifie la partition de démarrage et signale les échecs au lieu de relancer l'ancien firmware. Le menu Système affiche la version installée.";

    const char* const CHANGELOG_TR =
        "- Düzeltme: OTA, önyükleme bölümünü doğrular ve eski yazılıma sessizce dönmek yerine hataları gösterir. Sistem menüsü yüklü sürümü gösterir.";

    const char* const CHANGELOG_ES =
        "- Corrección: OTA verifica la partición de arranque y muestra errores en lugar de reiniciar el firmware anterior sin aviso. El menú Sistema muestra la versión instalada.";

    const char* const CHANGELOG_IT =
        "- Correzione: OTA verifica la partizione di avvio e mostra gli errori invece di riavviare senza avviso il vecchio firmware. Il menu Sistema mostra la versione installata.";

    const char* const CHANGELOG_PT =
        "- Correção: OTA verifica a partição de inicialização e mostra falhas em vez de reiniciar o firmware antigo silenciosamente. O menu Sistema mostra a versão instalada.";

    const char* const CHANGELOG_NL =
        "- Fix: OTA controleert de opstartpartitie en toont fouten in plaats van ongemerkt de oude firmware te starten. Het systeemmenu toont de geïnstalleerde versie.";

    const char* const TABLE[CHANGELOG_LANG_COUNT] = {
        CHANGELOG_EN, CHANGELOG_DE, CHANGELOG_FR, CHANGELOG_TR, CHANGELOG_ES, CHANGELOG_IT, CHANGELOG_PT, CHANGELOG_NL
    };
}

const char* changelogLatest() {
    uint8_t lang = SettingsStore::language();
    if (lang >= CHANGELOG_LANG_COUNT) lang = 0;
    return TABLE[lang];
}

}
