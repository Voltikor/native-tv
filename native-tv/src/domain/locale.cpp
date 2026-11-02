#include "domain/locale.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

namespace nativetv {
namespace {
thread_local const Locale *activeLocale = nullptr;
Json readDocument(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return Json();
    std::string bytes;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
        bytes.append(buffer, static_cast<size_t>(file.gcount()));
        if (bytes.size() > 2 * 1024 * 1024) return Json();
    }
    return Json::parse(bytes);
}
std::string fill(const std::string &text, const Json &variables) {
    std::string result;
    for (size_t i = 0; i < text.size();) {
        size_t end = text[i] == '{' ? text.find('}', i + 1) : std::string::npos;
        if (end != std::string::npos) {
            const Json value = variables[text.substr(i + 1, end - i - 1)];
            if (value.valid()) {
                result += value.string();
                i = end + 1;
                continue;
            }
        }
        result += text[i++];
    }
    return result;
}
} // namespace
Locale::Locale(const std::string &directory, const std::string &systemLocale)
    : system_(resolve(systemLocale)) {
    if (!directory.empty()) {
        Json catalog = readDocument(directory + "/pt-BR.json");
        if (catalog.isObject()) catalogs_["pt-BR"] = catalog;
    }
}
std::string Locale::resolve(const std::string &tag) {
    std::string value = tag;
    std::replace(value.begin(), value.end(), '_', '-');
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    // Only Brazilian Portuguese is shipped. A generic Portuguese TV uses it;
    // other regional variants fall back to English until their catalog exists.
    if (value == "pt" || value == "pt-br" || value.compare(0, 6, "pt-br.") == 0)
        return "pt-BR";
    return "en";
}
std::string Locale::contentLanguage(const std::string &tag) {
    return resolve(tag) == "pt-BR" ? "pt-BR" : "en-US";
}
std::string Locale::pluralCategory(const std::string &tag, uint64_t count) {
    // CLDR cardinal rules for the integer counts the launcher displays.
    if (resolve(tag) == "pt-BR") {
        if (count <= 1) return "one";
        if (count % 1000000 == 0) return "many";
    } else if (count == 1) return "one";
    return "other";
}
std::string Locale::language() const { return choice_ == "auto" ? system_ : choice_; }
bool Locale::choose(const std::string &choice, std::string *error) {
    if (choice != "auto" && choice != "en" && choice != "pt-BR") {
        if (error) *error = "Unsupported language";
        return false;
    }
    choice_ = choice;
    return true;
}
std::string Locale::message(const std::string &key, const std::string &english,
                           const Json &variables) const {
    const auto catalog = catalogs_.find(language());
    if (catalog != catalogs_.end()) {
        const Json entry = catalog->second[key];
        if (entry["from"].string() == english && entry["text"].isString() &&
            !entry["text"].string().empty())
            return fill(entry["text"].string(), variables);
    }
    return fill(english, variables);
}
std::string Locale::plural(const std::string &key, const std::string &one,
                          const std::string &other, uint64_t count) const {
    std::string text = count == 1 ? one : other;
    const auto catalog = catalogs_.find(language());
    if (catalog != catalogs_.end()) {
        const Json entry = catalog->second[key];
        const Json forms = entry["text"];
        if (entry["from"]["one"].string() == one && entry["from"]["other"].string() == other) {
            std::string form = forms[pluralCategory(language(), count)].string();
            if (form.empty()) form = forms["other"].string();
            if (!form.empty()) text = form;
        }
    }
    Json values = Json::object();
    values.set("count", Json::text(std::to_string(count)));
    return fill(text, values);
}
std::string Locale::decimal(double value, int digits) const {
    if (!std::isfinite(value)) return "—";
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(std::max(0, std::min(6, digits))) << value;
    std::string text = out.str();
    if (language() == "pt-BR") std::replace(text.begin(), text.end(), '.', ',');
    return text;
}
LocaleScope::LocaleScope(const Locale *locale) : previous_(activeLocale) { activeLocale = locale; }
LocaleScope::~LocaleScope() { activeLocale = previous_; }
const Locale *currentLocale() { return activeLocale; }
std::string localizedPlural(const std::string &key, const std::string &one,
                            const std::string &other, uint64_t count) {
    const Locale english;
    return (activeLocale ? activeLocale : &english)->plural(key, one, other, count);
}
std::string localizedDecimal(double value, int digits) {
    const Locale english;
    return (activeLocale ? activeLocale : &english)->decimal(value, digits);
}
std::string localizedInteger(long long value) {
    const std::string digits = std::to_string(value);
    const size_t start = value < 0 ? 1 : 0;
    const char separator = activeLocale && activeLocale->language() == "pt-BR" ? '.' : ',';
    std::string result;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i > start && (digits.size() - i) % 3 == 0) result += separator;
        result += digits[i];
    }
    return result;
}
} // namespace nativetv
