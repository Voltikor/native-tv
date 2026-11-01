#include "domain/say.hpp"
#include "domain/models.hpp"

namespace nativetv {
std::string setupTitle(const std::string &step) {
    if (step == "network")
        return say::msg("setup.net.title", "Turn on the web dashboard?");
    if (step == "ha")
        return say::msg("setup.ha.title", "Connect to Home Assistant?");
    if (step == "handoff")
        return say::msg("launcher.setup.finishOnAPhone", "Finish on a phone");
    if (step == "standby")
        return say::msg("setup.standby.title", "Stay connected when off?");
    if (step == "summary")
        return say::msg("setup.summary.title", "Ready to go");
    return say::msg("setup.slogan", "Your TV, your rules.");
}
std::vector<MenuRow> setupRows(const std::string &step, const Json &state) {
    if (step == "welcome")
        return {{say::msg("setup.start", "Get started"), "",
                 say::msg("launcher.setup.readingsControlsAndPrivacySettingsForThis", "Readings, controls and privacy settings for this TV, with the remote."), "",
                 navigate("setup:network")}};
    if (step == "network")
        return {{say::msg("setup.net.open", "Yes, on phones and computers at home"), "",
                 state["token"].boolean() ? say::msg("launcher.setup.theDashboardAccessTokenIsRequired", "The dashboard access token is required."
                                          ): say::msg("launcher.setup.anyoneOnTheHomeNetworkCanUse", "Anyone on the home network can use the dashboard."),
                 "",
                 post(Operation::Setup,
                      fields({{"action", Json::text("network")}, {"open", Json::flag(true)}}))},
                {say::msg("setup.net.closed", "No, only on this TV"), "", say::msg("launcher.setup.theDashboardRemainsAvailableOnTheTV", "The dashboard remains available on the TV."), "",
                 post(Operation::Setup,
                      fields({{"action", Json::text("network")}, {"open", Json::flag(false)}}))}};
    if (step == "ha")
        return {{say::msg("launcher.setup.setItUpOnAPhone", "Set it up on a phone"), "", say::msg("launcher.setup.connectAnMQTTBrokerForHomeAssistant", "Connect an MQTT broker for Home Assistant."), "",
                 navigate("setup:handoff")},
                {say::msg("setup.ha.later", "Not now"), "", say::msg("launcher.setup.homeAssistantCanBeSetUpLater", "Home Assistant can be set up later in Settings."), "",
                 navigate(state.has("alwaysReady") ? "setup:standby" : "setup:summary")}};
    if (step == "handoff")
        return {{say::msg("launcher.setup.createANewLink", "Create a new link"), "", say::msg("launcher.setup.scanTheQRCodeWithAPhone", "Scan the QR code with a phone camera."), "",
                 post(Operation::Setup, fields({{"action", Json::text("handoff")}}))},
                {say::msg("launcher.setup.continue", "Continue"), "", "", "",
                 navigate(state.has("alwaysReady") ? "setup:standby" : "setup:summary")}};
    if (step == "standby")
        return {{say::msg("setup.standby.on", "Yes, keep it connected"), "",
                 say::msg("launcher.setup.keepsTheNetworkAvailableWithTheScreen", "Keeps the network available with the screen dark. Uses about 12 W on a C2, apart "
                 "from five hours of full sleep each night."),
                 "",
                 post(Operation::Setup,
                      fields({{"action", Json::text("alwaysReady")}, {"on", Json::flag(true)}}))},
                {say::msg("setup.standby.off", "No, let it sleep"), "",
                 say::msg("launcher.setup.nativeTvAndHomeAssistantWaitUntil", "Native Tv and Home Assistant wait until the TV is on again."), "",
                 post(Operation::Setup,
                      fields({{"action", Json::text("alwaysReady")}, {"on", Json::flag(false)}}))}};
    return {
        {say::msg("tv.settings.webDashboard", "Web dashboard"), state["network"].boolean() ? say::msg("common.on", "On" ): say::msg("common.off", "Off"), "", "", Command()},
        {"Home Assistant", state["homeAssistant"]["state"].string(say::msg("tv.settings.ha.notSetUp", "Not set up")), "", "", Command()},
        {say::msg("setup.summary.open", "Open dashboard"), "", "", "",
         post(Operation::Setup, fields({{"action", Json::text("done")}}))}};
}
} // namespace nativetv
