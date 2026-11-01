#include "ui/screen.hpp"
#include "ui/kit/overlay.hpp"
#include "domain/say.hpp"

namespace nativetv {
namespace {
const char *const choices[] = {"auto", "en", "pt-BR"};
class LanguageScreen final : public ScreenView {
    size_t selected_ = 0;
  public:
    std::string name() const override { return "language"; }
    bool hasBar() const override { return false; }
    void enter(ScreenHost &host) override {
        for (size_t i = 0; i < 3; ++i)
            if (host.languageChoice() == choices[i]) selected_ = i;
    }
    bool key(ScreenHost &host, Key key) override {
        if (key == Key::Back) return false;
        if (key == Key::Up && selected_) --selected_;
        if (key == Key::Down && selected_ < 2) ++selected_;
        if (key == Key::Ok) host.changeLanguage(choices[selected_]);
        host.invalidate();
        return true;
    }
    void click(ScreenHost &host, const std::string &id) override {
        for (size_t i = 0; i < 3; ++i)
            if (id == std::string("language:") + choices[i]) {
                selected_ = i;
                host.changeLanguage(choices[i]);
                return;
            }
    }
    void build(ScreenHost &host, kit::Paint paint) override {
        kit::PageHead head;
        head.title = say::msg("launcher.language.title", "Language");
        head.subtitle = say::msg("launcher.language.description",
            "Choose the language of the launcher and movie information.");
        kit::pageHead(paint, head);
        const std::string labels[] = {
            say::msg("launcher.language.auto", "Automatic — follow TV"),
            "English", "Português (Brasil)"};
        for (size_t i = 0; i < 3; ++i) {
            kit::PillButton button;
            button.rest = {kit::canvas::safeX, 320 + static_cast<float>(i) * 112, 1000, 80};
            button.id = std::string("language:") + choices[i];
            button.label = labels[i];
            button.icon = host.languageChoice() == choices[i] ? "check" : "globe";
            button.focus = selected_ == i ? 1 : 0;
            kit::pillButton(paint, button);
        }
        kit::keyHint(paint, kit::canvas::safeX, 1002,
                     say::msg("launcher.key.back", "Back"),
                     say::msg("tv.details.return", "Return"), "back");
    }
};
} // namespace
std::unique_ptr<ScreenView> makeLanguageScreen(const std::string &route) {
    return route == "language" ? std::unique_ptr<ScreenView>(new LanguageScreen()) : nullptr;
}
} // namespace nativetv
