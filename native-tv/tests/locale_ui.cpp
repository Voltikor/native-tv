#include "overlay_harness.hpp"
#include "domain/settings_items.hpp"
#include <fstream>
#include <iterator>

using namespace overlay_test;
static Json fixture(const std::string &path) {
    std::ifstream file(std::string(NATIVETV_FIXTURES) + "/tv/" + path, std::ios::binary);
    Json result = Json::parse(std::string(std::istreambuf_iterator<char>(file), {}));
    check(result.valid(), "fixture must parse");
    return result;
}
static void controls(Locale &locale) {
    Json caps = Json::object();
    caps.set("allowPower", Json::flag(true));
    const auto groups = settingGroups();
    for (const auto &group : groups) {
        Json data;
        if (group.key == "system" || group.key == "options")
            data = fixture("stats-c4-webos9.json");
        else
            data = fixture("dashboard/" + (group.key == "settings" ? std::string("setup") : group.key) + ".json");
        if (group.key == "options") {
            Json extra = fixture("dashboard/options-extra.json");
            for (const auto &key : extra.keys()) data.set(key, extra[key]);
        }
        check(locale.choose("en"), "choose English");
        const auto english = settingItems(group.key, dashboardRows(group.key, data, caps));
        check(locale.choose("pt-BR"), "choose Portuguese");
        const auto portuguese = settingItems(group.key, dashboardRows(group.key, data, caps));
        check(english.size() == portuguese.size(), "translation preserves settings topology");
        for (size_t i = 0; i < english.size(); ++i) {
            const auto &a = english[i], &b = portuguese[i];
            check(a.kind == b.kind && a.on == b.on, "translation preserves control type and state");
            check(a.command.operation == b.command.operation && a.command.route == b.command.route &&
                      a.command.body.dump() == b.command.body.dump(), "translation preserves commands and raw values");
            check(a.choices.size() == b.choices.size(), "translation preserves choices");
            for (size_t j = 0; j < a.choices.size(); ++j)
                check(a.choices[j].second.body.dump() == b.choices[j].second.body.dump(), "choice values stay language-neutral");
        }
    }
}
int main(int argc, char **argv) {
    try {
        check(argc == 2, "catalog directory required");
        Locale locale(argv[1], "pt-BR");
        LocaleScope scope(&locale);
        controls(locale);
        FakeTransport transport;
        transport.language(locale.language());
        Application app(transport, "home", &locale);
        app.response({"caps", "/api/caps", R"({"ok":true,"setupNeeded":false})", "", 1, 200});
        app.response({"catalogue", "/api/home/catalogue", catalogueJson, "", 1, 200});
        app.response({"discover", "/api/home/discover", feedJson, "", 1, 200});
        run(app, 0, 1000);
        app.open("language");
        check(app.shows("Idioma"), "language picker is translated");
        check(app.activate("language:en"), "language selection is reachable by pointer");
        app.update(1100);
        const auto enWrite = transport.last("w:locale");
        check(enWrite.operation == Operation::SetHomeLayout && enWrite.body["language"].string() == "en",
              "language is saved through the daemon, outside the app jail");
        check(locale.language() == "pt-BR", "UI waits until saving is confirmed");
        app.response({"w:locale", "/api/home/layout", R"({"ok":true})", "", enWrite.generation, 200});
        app.update(1120);
        check(locale.choice() == "en" && transport.requestLanguage == "en", "selection updates UI and request locale");
        check(app.shows("Language"), "picker rebuilds without a restart");
        check(app.activate("language:pt-BR"), "Portuguese choice is reachable");
        app.update(1200);
        app.response({"w:locale", "/api/home/layout", R"({"ok":true})", "", transport.last("w:locale").generation, 200});
        app.update(1220);
        check(app.shows("Idioma") && locale.choice() == "pt-BR", "switch back without stale strings");
        app.activate("language:auto");
        app.update(1300);
        app.response({"w:locale", "/api/home/layout", R"({"ok":false})", "", transport.last("w:locale").generation, 200});
        app.update(1320);
        check(locale.choice() == "pt-BR", "failed save preserves the previous language");
        app.key(Key::Back);
        run(app, 1200, 500);
        check(app.screenName() == "home", "Back returns to Home after switching");
        for (const auto &request : transport.requests)
            check(!request.write || (request.operation == Operation::SetHomeLayout && request.body.keys().size() == 1 &&
                  request.body["language"].isString()), "language switching writes only the language preference");
        check(appKindTitle("tools", "Tools") == "Ferramentas", "default app kinds are translated");
        check(appKindTitle("tools", "My custom category") == "My custom category", "custom names remain intact");
        {
            Locale reloaded(argv[1], "pt-BR");
            LocaleScope fresh(&reloaded);
            FakeTransport reconnected;
            Application restarted(reconnected, "home", &reloaded);
            Json saved = Json::parse(catalogueJson);
            saved["layout"].set("language", Json::text("en"));
            restarted.response({"catalogue", "/api/home/catalogue", saved.dump(), "", 1, 200});
            restarted.update(10);
            check(reloaded.choice() == "en" && reconnected.requestLanguage == "en", "saved language is restored from the server on relaunch");
            check(restarted.screenName() == "home", "restoring a language does not open the picker");
        }
        std::printf("%d locale UI checks passed\n", checkCount());
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
