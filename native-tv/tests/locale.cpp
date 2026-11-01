#include "domain/locale.hpp"
#include "domain/say.hpp"
#include <cstdio>
#include <stdexcept>
#include <thread>

using namespace nativetv;
static void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        check(argc == 2, "catalog directory required");
        Locale locale(argv[1], "", "pt_BR.UTF-8");
        check(locale.choice() == "auto" && locale.language() == "pt-BR", "follow TV locale");
        check(Locale::resolve("PT-br") == "pt-BR", "case-insensitive language tag");
        check(Locale::resolve("de-DE") == "en", "unsupported language falls back");
        check(Locale::resolve("pt-PT") == "en", "regional variant is not silently Brazilian");
        check(Locale::contentLanguage("pt-BR") == "pt-BR", "content locale");
        check(Locale::contentLanguage("../pt-BR\r\n") == "en-US", "reject malformed locale");
        LocaleScope scope(&locale);
        check(say::msg("launcher.home.title", "Home") == "Início", "direct lookup by key");
        check(say::msg("launcher.home.title", "Changed English") == "Changed English", "stale translation falls back");
        check(say::msg("missing.key", "English") == "English", "missing translation falls back");
        Json variables = Json::object();
        variables.set("name", Json::text("{other}"));
        check(say::msg("missing.key", "Hello {name}", variables) == "Hello {other}", "values are not parsed as templates");
        check(Locale::pluralCategory("en", 0) == "other", "English zero plural");
        check(Locale::pluralCategory("en", 1) == "one", "English singular");
        check(Locale::pluralCategory("pt-BR", 0) == "one", "Brazilian Portuguese zero category");
        check(Locale::pluralCategory("pt-BR", 2) == "other", "Brazilian Portuguese plural");
        check(Locale::pluralCategory("pt-BR", 1000000) == "many", "CLDR million category");
        check(localizedPlural("launcher.content.seasons", "{count} season", "{count} seasons", 1) == "1 temporada", "translated singular");
        check(localizedPlural("launcher.content.seasons", "{count} season", "{count} seasons", 2) == "2 temporadas", "translated plural");
        check(localizedDecimal(6.4, 1) == "6,4", "Brazilian decimal separator");
        bool workerEnglish = false;
        std::thread worker([&] { workerEnglish = say::msg("launcher.home.title", "Home") == "Home"; });
        worker.join();
        check(workerEnglish, "UI locale does not leak to HTTP workers");
        {
            LocaleScope english(nullptr);
            check(say::msg("launcher.home.title", "Home") == "Home", "nested English scope");
        }
        check(say::msg("launcher.home.title", "Home") == "Início", "scope restores context");
        check(locale.choose("en") && locale.language() == "en", "switch language without restart");
        check(localizedPlural("launcher.content.seasons", "{count} season", "{count} seasons", 1) == "1 season", "English plural fallback");
        check(!locale.choose("bad") && locale.language() == "en", "invalid preference preserves choice");
        const std::string path = "locale-test-preference.json";
        Locale persistent(argv[1], path, "en");
        check(persistent.choose("pt-BR"), "persist preference atomically");
        Locale reloaded(argv[1], path, "en");
        check(reloaded.language() == "pt-BR", "reload saved preference");
        std::remove(path.c_str());
        Locale unwritable(argv[1], "/nonexistent-native-tv-directory/locale.json", "en");
        check(!unwritable.choose("pt-BR") && unwritable.choice() == "auto", "failed write preserves preference");
        std::puts("Locale checks passed");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
