#pragma once
#include "core/json.hpp"
#include <cstdint>
#include <map>
#include <string>

namespace nativetv {
// One native client's locale. Catalogs are read once; lookups never touch disk.
// English lives beside its key in the source, matching the web dashboard.
class Locale {
    std::map<std::string, Json> catalogs_;
    std::string choice_ = "auto", system_ = "en";
  public:
    explicit Locale(const std::string &directory = "",
                    const std::string &systemLocale = "en");
    static std::string resolve(const std::string &tag);
    static std::string contentLanguage(const std::string &tag);
    static std::string pluralCategory(const std::string &tag, uint64_t count);
    std::string language() const;
    const std::string &choice() const { return choice_; }
    bool choose(const std::string &choice, std::string *error = nullptr);
    std::string message(const std::string &key, const std::string &english,
                        const Json &variables = Json()) const;
    std::string plural(const std::string &key, const std::string &one,
                       const std::string &other, uint64_t count) const;
    std::string decimal(double value, int digits = 0) const;
};

// The client establishes this scope on its UI thread. HTTP workers and the daemon
// keep their own language; existing say::msg calls resolve their keys directly.
class LocaleScope {
    const Locale *previous_;
  public:
    explicit LocaleScope(const Locale *locale);
    ~LocaleScope();
    LocaleScope(const LocaleScope &) = delete;
    LocaleScope &operator=(const LocaleScope &) = delete;
};
const Locale *currentLocale();
std::string localizedPlural(const std::string &key, const std::string &one,
                            const std::string &other, uint64_t count);
std::string localizedDecimal(double value, int digits = 0);
std::string localizedInteger(long long value);
} // namespace nativetv
