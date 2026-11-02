#include "domain/say.hpp"
#include "domain/say_translation.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace nativetv;

namespace {
unsigned checks = 0;

void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}

void write(const std::string &path, const std::string &contents) {
    std::ofstream(path.c_str(), std::ios::binary) << contents;
}

Json one(const char *name, const std::string &value) {
    Json variables = Json::object();
    variables.set(name, Json::text(value));
    return variables;
}

// Dictionaries on disk, as the daemon reads them from assets/i18n/.
struct Dictionaries {
    std::string directory;
    Dictionaries() {
        char pattern[] = "/tmp/nativetv-say-XXXXXX";
        directory = mkdtemp(pattern);
        write(directory + "/es.json",
              "{\"srv.saver.switching\":{\"text\":\"La TV aun cambia de salvapantallas.\","
              "\"from\":\"The TV is still switching screen savers.\"},"
              "\"srv.update.failed\":{\"text\":\"Fallo la actualizacion: {error}\","
              "\"from\":\"The update failed: {error}\"},"
              "\"srv.power.on\":{\"text\":\"Encendido\",\"from\":\"On\"},"
              "\"srv.power.off\":{\"text\":\"Apagado\",\"from\":\"Off\"},"
              "\"srv.old\":{\"text\":\"Texto viejo\",\"from\":\"The old words\"},"
              "\"srv.list\":{\"text\":\"{items} y {last}\",\"from\":\"{items} and {last}\"},"
              "\"srv.empty\":{\"text\":\"\",\"from\":\"Never empty\"},"
              "\"srv.badshape\":\"just a string\"}");
        write(directory + "/de.json", "{ this is not json");
        write(directory + "/english.json", "{}");
        write(directory + "/fr.txt", "{}");
    }
    ~Dictionaries() {
        for (const char *name : {"es.json", "de.json", "english.json", "fr.txt"})
            unlink((directory + "/" + name).c_str());
        rmdir(directory.c_str());
    }
};
} // namespace

int main() {
    try {
        check(say::msg("srv.power.on", "On") == "On", "English fallback");
        check(say::msg("srv.update.failed", "The update failed: {error}",
                       Json::parse("{\"error\":\"disk full\"}")) ==
                  "The update failed: disk full",
              "Message placeholder substitution");
        check(say::msg("srv.update.failed", "The update failed: {error}", Json::object()) ==
                  "The update failed: {error}",
              "Missing placeholder remains intact");
        check(say::msg("srv.privacy.running", "{name} is running",
                       Json::parse("{\"name\":\"Diagnostics uploader\"}")) ==
                  "Diagnostics uploader is running",
              "Nested message text substitution");
        check(say::list({}) == "" && say::list({"Only"}) == "Only" &&
                  say::list({"Terms", "Privacy", "Ads"}) == "Terms, Privacy and Ads",
              "Natural-language lists");

        // Before any dictionary is loaded nothing is translated.
        const std::string saver = say::msg("srv.saver.switching", "The TV is still switching screen savers.");
        const std::string body = "{\"ok\":false,\"error\":\"" + saver + "\"}";
        check(say::translateJson(body, "es") == body, "No dictionary, no translation");
        check(say::requestedLanguage("es").empty(), "No dictionary, no language");

        const Dictionaries dictionaries;
        check(say::loadTranslations(dictionaries.directory) == 1,
              "Only well-formed <xx>.json files with entries are read");
        check(say::loadTranslations("/nonexistent/nativetv-say") == 0 && say::requestedLanguage("es").empty(),
              "A missing folder leaves English");
        check(say::loadTranslations(dictionaries.directory) == 1, "Reading again replaces what was read");

        check(say::requestedLanguage("es") == "es" && say::requestedLanguage("ES") == "es",
              "A language with a dictionary is accepted, in either case");
        check(say::requestedLanguage("").empty() && say::requestedLanguage("en").empty() &&
                  say::requestedLanguage("de").empty() && say::requestedLanguage("fr").empty() &&
                  say::requestedLanguage("es-MX").empty() && say::requestedLanguage("../").empty(),
              "English, a language without a dictionary and odd values are not");

        // Messages made after the load are remembered, and swapped in display fields only.
        const std::string made = say::msg("srv.saver.switching", "The TV is still switching screen savers.");
        std::string expected = "{\"ok\":false,\"error\":\"La TV aun cambia de salvapantallas.\"}";
        check(say::translateJson("{\"ok\":false,\"error\":\"" + made + "\"}", "es") == expected,
              "A fixed message in a display field is translated");
        check(say::translateJson("{\"state\":\"" + made + "\"}", "es") == "{\"state\":\"" + made + "\"}",
              "The same words in a field a page does not show are left alone");
        say::msg("srv.power.on", "On");
        check(say::translateJson("{\"label\":\"On\",\"state\":\"On\",\"on\":true}", "es") ==
                  "{\"label\":\"Encendido\",\"state\":\"On\",\"on\":true}",
              "A raw value that reads like a message is never changed");

        const std::string failed = say::msg("srv.update.failed", "The update failed: {error}", one("error", "disk full"));
        check(say::translateJson("{\"error\":\"" + failed + "\"}", "es") ==
                  "{\"error\":\"Fallo la actualizacion: disk full\"}",
              "A message with values keeps them");
        const std::string nested =
            say::msg("srv.update.failed", "The update failed: {error}", one("error", saver));
        check(say::translateJson("{\"error\":\"" + nested + "\"}", "es") ==
                  "{\"error\":\"Fallo la actualizacion: La TV aun cambia de salvapantallas.\"}",
              "A value that is a message itself is translated too");

        const std::string joined = say::list({say::msg("srv.power.on", "On"), say::msg("srv.power.off", "Off"), "Wi-Fi"});
        check(say::translateJson("{\"error\":\"" + joined + "\"}", "es") ==
                  "{\"error\":\"Encendido, Apagado y Wi-Fi\"}",
              "A list joins translated items, and leaves a name the TV gave as it is");

        // Lists, objects inside lists and nesting.
        check(say::translateJson("{\"rows\":[{\"label\":\"On\",\"id\":\"On\"},{\"label\":\"Off\"}],"
                                 "\"sharesWith\":[\"On\",\"someone\"],\"n\":3,\"f\":1.5,\"x\":null}", "es") ==
                  "{\"rows\":[{\"label\":\"Encendido\",\"id\":\"On\"},{\"label\":\"Apagado\"}],"
                  "\"sharesWith\":[\"Encendido\",\"someone\"],\"n\":3,\"f\":1.5,\"x\":null}",
              "Rows, lists of names and other values come through");
        check(say::translateJson("[{\"name\":\"Off\"}]", "es") == "[{\"name\":\"Off\"}]",
              "A name is a raw value by default, including in a list at the top");

        // These words are already known messages; matching them must not rename user content.
        const std::string namedContent = R"({"name":"On","title":"Off",
            "profiles":[{"id":"guest","name":"Off"}],
            "apps":[{"id":"my.app","name":"On","title":"Off"}],
            "installed":[{"id":"my.app","title":"Off"}],
            "systemTiles":[{"id":"system.app","title":"On"}],
            "install":{"preview":{"title":"Off"},"result":{"title":"On"}},
            "quickAccess":{"keys":[{"title":"On"}],"choices":[{"title":"Off"}]},
            "kind":"app","label":"On"})";
        Json namedExpected = Json::parse(namedContent);
        namedExpected.set("label", Json::text("Encendido"));
        check(say::translateJson(namedContent, "es") == namedExpected.dump(),
              "Profile and app names stay intact while neighbouring UI messages translate");
        for (const char *path : {"services", "rows", "lgSettings"}) {
            Json generated = Json::object();
            generated.set(path, Json::parse(R"([{"title":"On"}])"));
            check(Json::parse(say::translateJson(generated.dump(), "es"))[path]
                      [static_cast<size_t>(0)]["title"].string() == "Encendido",
                  "Server-owned settings and service titles still translate");
        }
        const Json generatedNames = Json::parse(say::translateJson(
            R"({"categories":[{"name":"On"}],"mqtt":{"categories":[{"name":"Off"}]},
                "simple":{"areas":[{"name":"On"}]},"homeScreen":{"parts":[{"title":"Off"}]}})", "es"));
        check(generatedNames["categories"][static_cast<size_t>(0)]["name"].string() == "Encendido" &&
                  generatedNames["mqtt"]["categories"][static_cast<size_t>(0)]["name"].string() == "Apagado" &&
                  generatedNames["simple"]["areas"][static_cast<size_t>(0)]["name"].string() == "Encendido" &&
                  generatedNames["homeScreen"]["parts"][static_cast<size_t>(0)]["title"].string() == "Apagado",
              "Server-owned category names, privacy areas and Home parts still translate");

        // English that changed since the translation was made, and entries that are not usable.
        say::msg("srv.old", "The old words, now different");
        check(say::translateJson("{\"error\":\"The old words, now different\"}", "es") ==
                  "{\"error\":\"The old words, now different\"}",
              "A translation made from other English shows the English");
        say::msg("srv.empty", "Never empty");
        say::msg("srv.badshape", "Bad shape");
        say::msg("srv.missing", "Not in the dictionary");
        const std::string unusable = "{\"error\":\"Never empty\",\"label\":\"Bad shape\",\"title\":\"Not in the dictionary\"}";
        check(say::translateJson(unusable, "es") == unusable, "An empty, malformed or missing entry shows the English");

        // Bodies that are not for translating come back as they were.
        check(say::translateJson("not json", "es") == "not json" && say::translateJson("", "es").empty(),
              "Anything that is not JSON is returned as it came");
        check(say::translateJson(body, "en") == body && say::translateJson(body, "de") == body,
              "A language with no dictionary leaves the body");
        const std::string plain = "{ \"error\" : \"nothing to change\" , \"url\":\"http://tv/a/b\" }";
        check(say::translateJson(plain, "es") == plain, "A body with nothing to change is not rewritten");

        // Memory stays bounded: a message with values is forgotten after enough others, a fixed one outlasts them.
        const std::string first = say::msg("srv.update.failed", "The update failed: {error}", one("error", "first"));
        for (int i = 0; i < 1500; ++i)
            say::msg("srv.update.failed", "The update failed: {error}", one("error", "e" + std::to_string(i)));
        const std::string latest = say::msg("srv.update.failed", "The update failed: {error}", one("error", "latest"));
        check(say::translateJson("{\"error\":\"" + first + "\"}", "es") == "{\"error\":\"" + first + "\"}",
              "The oldest message with values is forgotten once the table fills");
        check(say::translateJson("{\"error\":\"" + latest + "\"}", "es") ==
                  "{\"error\":\"Fallo la actualizacion: latest\"}",
              "and a recent one is still translated");

        // msg() runs on every HTTP worker while responses are translated.
        std::vector<std::thread> workers;
        std::vector<int> failures(4, 0);
        for (int worker = 0; worker < 4; ++worker)
            workers.emplace_back([worker, &failures] {
                for (int i = 0; i < 3000; ++i) {
                    const std::string value = std::to_string(worker) + "-" + std::to_string(i);
                    const std::string text = say::msg("srv.update.failed", "The update failed: {error}", one("error", value));
                    const std::string fixed = say::msg("srv.power.on", "On");
                    const std::string out = say::translateJson(
                        "{\"error\":\"" + text + "\",\"label\":\"" + fixed + "\"}", "es");
                    if (out.find("Encendido") == std::string::npos) ++failures[worker];
                }
            });
        for (std::thread &worker : workers)
            worker.join();
        check(failures[0] + failures[1] + failures[2] + failures[3] == 0,
              "Concurrent messages and translations stay consistent");

        // Loading nothing turns it off again.
        check(say::loadTranslations("/nonexistent/nativetv-say") == 0 && say::translateJson(body, "es") == body,
              "Without a dictionary nothing is remembered or translated");

        std::printf("%u native message checks passed\n", checks);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
