#include "overlay_harness.hpp"
#include "ui/home_layout.hpp"

using namespace overlay_test;

namespace {
std::string apps() {
    return R"({"ok":true,"profile":"main","apps":[{"id":"a","title":"Alpha","kind":"video"},{"id":"b","title":"Beta","kind":"music"},{"id":"c","title":"Gamma","kind":"video"},{"id":"d","title":"Delta","kind":"music"}],"kinds":[{"key":"video","label":"Movies & TV"},{"key":"music","label":"Music & audio"}],"order":{"app":["a","b","c","d"]},"layout":{"rows":["app"],"hidden":{},"showLgHome":false,"showInputs":false},"profiles":[{"id":"main","name":"Main","active":true}]})";
}

void shapeChecks() {
    for (const char *shape : {"round", "square", "squircle", "rectangle"}) {
        Json catalogue = Json::parse(apps());
        catalogue["layout"].set("appShape", Json::text(shape));
        CardRow row = appsRow(catalogue, Json::object(), false);
        check(row.appShape == shape, "The saved app shape reaches Home's row model");
        World world(catalogue.dump().c_str());
        world.settle(600);
        size_t curved = 0;
        for (const Node &node : world.app.scene().nodes) if (node.squircle) ++curved;
        check((curved > 0) == (std::string(shape) == "squircle"),
              "Only Squircle selects the continuous curve on the actual scene nodes");
        const Rect first = home::item(row, 0, 0, true), second = home::item(row, 0, 1, true);
        const bool wide = std::string(shape) == "rectangle";
        check(std::fabs(first.w / first.h - (wide ? 16.f / 9 : 1.f)) < .001f,
              "Tile geometry uses the requested aspect ratio");
        check(second.x > first.x + first.w, "Adjacent rectangular tiles never overlap");
        check(row.cards[0].image == (wide ? "banner:a" : "app:a"),
              "Wide tiles request banner art; square and round tiles share square art");
    }
}

void dimmingChecks() {
    struct Case { const char *mode; bool oled, dims; };
    for (const Case &test : {Case{"auto", false, false}, Case{"auto", true, true},
                             Case{"on", false, true}, Case{"off", true, false}}) {
        Json catalogue = Json::parse(apps());
        catalogue.set("oledPanel", Json::flag(test.oled));
        Json layout = catalogue["layout"];
        layout.set("idleDimming", Json::text(test.mode));
        World world(catalogue.dump().c_str());
        world.settle(34000);
        check(world.snapshot()["idle"].boolean() == test.dims, "Idle dimming follows Auto/On/Off and the panel type");
    }
}

void rowMemoryChecks() {
    World world(apps().c_str());
    Json items = Json::array();
    for (int i = 1; i <= 14; ++i)
        items.add(fields({{"id", Json::numeric(i)}, {"type", Json::text("movie")}, {"title", Json::text("Movie " + std::to_string(i))}}));
    Json rows = Json::array();
    rows.add(fields({{"key", Json::text("trending")}, {"items", items}}));
    world.reply("discover", fields({{"ok", Json::flag(true)}, {"enabled", Json::flag(true)}, {"titlesEnabled", Json::flag(true)}, {"rows", rows}}).dump());
    world.settle(500);
    world.press({Key::Right, Key::Right, Key::Right});
    check(world.snapshot()["column"].number() == 3, "The fourth app is focused");
    world.press(Key::Down);
    check(world.snapshot()["column"].number() == 0, "An unvisited row starts on its first item");
    world.press({Key::Right, Key::Right});
    const std::string movie = world.snapshot()["focusCard"].string();
    world.settle(700);
    for (const char *channel : {"catalogue", "discover", "catalogue"}) {
        world.reply(channel, std::string(channel) == "catalogue" ? apps() :
            fields({{"ok", Json::flag(true)}, {"enabled", Json::flag(true)},
                    {"titlesEnabled", Json::flag(true)}, {"rows", rows}}).dump());
        world.settle(700);
        check(world.snapshot()["focusCard"].string() == movie &&
                  world.motion("f:1.2") > .99 && world.motion("f:1.7") < .01,
              "Refreshing a split highlights row keeps the visible focus on its saved movie");
        check(world.snapshot()["heroTitle"].string() == "Movie 9",
              "The highlights focus and hero still name the same movie after refresh");
    }
    world.press(Key::Up);
    check(world.snapshot()["focusCard"].string() == "d", "The apps row remembers its own item");
    world.press(Key::Down);
    check(world.snapshot()["focusCard"].string() == movie, "The movie row remembers a different item");
    world.press(Key::Up);
    Json refreshed = Json::parse(apps());
    refreshed.set("order", fields({{"app", Json::parse(R"(["d","a","b","c"])")}}));
    world.reply("catalogue", refreshed.dump());
    check(world.snapshot()["focusCard"].string() == "d" && world.snapshot()["column"].number() == 0,
          "A refreshed row follows the saved item id rather than its former index");
}

void barBackChecks() {
    for (bool search : {false, true})
    for (bool hold : {false, true}) {
        World world;
        world.press(Key::Up);
        for (int i = 0; i < 10; ++i) world.press(Key::Right);
        if (search) world.press(Key::Left);
        check(!world.app.canOpenRecents(), "Toolbar focus does not arm the recent-app hold gesture");
        if (hold) world.app.longBack(); else world.press(Key::Back);
        world.settle(500);
        check(world.screen() == "home" && world.snapshot()["tab"].string() == "home" &&
              world.snapshot()["nav"].number() == 0 && world.transport.count("recents") == 0,
              "Back and held Back from the gear return focus to Inicio");
    }
}

void carriedAppChecks() {
    World world(apps().c_str());
    world.press(Key::Up);
    world.press(Key::Right);
    world.press(Key::Ok, 1200);
    world.app.longPress();
    world.press(Key::Right);
    check(world.snapshot()["focusCard"].string() == "a" && world.snapshot()["rowHead"].string() == "c,a",
          "Holding OK immediately picks the app up so an arrow moves it without selecting its neighbour");
    world.reply("catalogue", apps());
    check(world.snapshot()["arranging"].boolean() && world.snapshot()["focusCard"].string() == "a",
          "A refresh preserves the app being carried");
    world.app.activate("card:0:0");
    check(world.snapshot()["focusCard"].string() == "a", "A pointer click puts the app down without selecting its neighbour");
    check(world.app.activate("tab:0"), "The Home tab is reachable while editing");
    world.settle(1200);
    check(world.snapshot()["tab"].string() == "home" && !world.snapshot()["arranging"].boolean(),
          "Switching tabs finishes category editing instead of carrying it onto Home");
}

void tabBackChecks() {
    for (uint32_t delay : {0u, 100u, 200u, 800u})
    for (Key next : {Key::Down, Key::Ok, Key::Back}) {
        World world(apps().c_str());
        world.press({Key::Up, Key::Right});
        world.settle(delay);
        world.press(Key::Back);
        check(world.snapshot()["nav"].number() == 0,
              "Back selects Inicio immediately, even while the old tab is leaving");
        world.press(next);
        if (next == Key::Back)
            world.press({Key::Back, Key::Back, Key::Back});
        world.settle(900);
        check(world.snapshot()["tab"].string() == "home" &&
                  world.snapshot()["nav"].number() == 0 && world.transport.count("recents") == 0,
              "Down, OK and repeated Back cannot reopen the previous tab after Back");
    }
}

void scrollingBarChecks() {
    for (bool appsTab : {false, true}) {
        Json catalogue = Json::parse(catalogueJson);
        if (appsTab) {
            Json entries = Json::array(), kinds = Json::array(), order = Json::array();
            for (int i = 0; i < 6; ++i) {
                std::string id = "kind" + std::to_string(i);
                order.add(Json::text(id));
                kinds.add(fields({{"key", Json::text(id)}, {"label", Json::text(id)}}));
                entries.add(fields({{"id", Json::text(id)}, {"title", Json::text(id)},
                                    {"kind", Json::text(id)}}));
            }
            catalogue.set("apps", entries);
            catalogue.set("kinds", kinds);
            catalogue.set("order", fields({{"app", order}}));
        }
        World world(catalogue.dump().c_str());
        Json feed = Json::parse(feedJson), rows = Json::array();
        for (const char *key : {"trending", "movies", "series"}) {
            Json row = Json::parse(feedJson)["rows"][size_t(0)];
            row.set("key", Json::text(key));
            rows.add(row);
        }
        feed.set("rows", rows);
        world.reply("discover", feed.dump());
        world.settle(500);
        if (appsTab) {
            check(world.app.activate("tab:1"), "Apps is reachable before scrolling");
            world.settle(900);
        }
        std::map<std::string, Rect> bar;
        for (const Hit &hit : world.app.scene().hits)
            if (hit.id.compare(0, 4, "nav:") == 0 || hit.id == "tool:2")
                bar[hit.id] = hit.rect;
        world.press({Key::Down, Key::Down, Key::Down});
        world.settle(900);
        double scroll = world.motion("sy");
        check(scroll < -180, "The content has scrolled far enough to hide the navigation row");
        for (const Hit &hit : world.app.scene().hits)
            if (bar.count(hit.id))
                check(std::fabs(hit.rect.y - bar[hit.id].y - scroll) < .5,
                      "Navigation pointer targets scroll with the page");
        bool gearMoved = false;
        for (const Node &node : world.app.scene().nodes)
            if (node.value == "local:icons/material/settings.svg")
                gearMoved = std::fabs(node.group.ty - scroll) < .5;
        check(gearMoved, "The navigation artwork scrolls with its pointer targets");
        for (int i = 0; i < 6; ++i) world.press(Key::Back, 300);
        world.settle(900);
        check(world.snapshot()["navigation"].boolean() && std::fabs(world.motion("sy")) < .5,
              "Returning to the navigation row brings it back into view");
    }
}

void homeResumeChecks() {
    for (const char *target : {"tab:0", "tab:1", "tool:1", "tool:2", "tool:0"}) {
        World world;
        check(world.app.activate(target), "The starting page is reachable");
        world.settle(900);
        const std::string before = world.screen();
        const double nav = world.snapshot()["nav"].number();
        world.app.active(true);
        check(world.screen() == before && world.snapshot()["nav"].number() == nav,
              "An already-active notification keeps the current page");
        world.app.active(false);
        world.app.active(true);
        check(std::fabs(world.motion("pgx")) < .001 &&
                  std::fabs(world.motion("ER:0")) < .001,
              "Home returns without blanking the page or restarting its entrance animation");
        world.settle(900);
        check(world.screen() == "home" && world.snapshot()["tab"].string() == "home" &&
                  world.snapshot()["navigation"].boolean() && world.snapshot()["nav"].number() == 0 &&
                  std::fabs(world.motion("sy")) < .5,
              "Returning through Home selects Inicio and brings its navigation into view");
        world.press(Key::Down, 900);
        check(world.snapshot()["tab"].string() == "home",
              "The next input stays on Inicio after Home resumes the app");
    }
}

void tabTransitionChecks() {
    for (const char *target : {"tab:0", "tab:1"}) {
        World world(apps().c_str());
        if (std::string(target) == "tab:0") {
            world.app.activate("tab:1");
            world.settle(900);
        }
        check(world.app.activate(target), "The destination tab is reachable");
        check(!world.app.canOpenRecents(), "A tab transition cannot arm the held-Back gesture");
        const Json leaving = world.snapshot();
        world.press({Key::Down, Key::Right, Key::Ok});
        world.app.activate("card:0:0");
        world.app.longPress();
        world.app.longBack();
        check(world.screen() == "home" && world.snapshot()["row"].number() == leaving["row"].number() &&
                  world.snapshot()["column"].number() == leaving["column"].number() &&
                  world.transport.count("action") == 0 && world.transport.count("recents") == 0,
              "Inputs during a tab transition cannot select or open an item on the leaving page");
        world.settle(900);
        check(world.snapshot()["tab"].string() == (std::string(target) == "tab:0" ? "home" : "apps"),
              "The destination tab appears without another input");
        world.press(Key::Right);
        check(world.snapshot()["column"].number() == 1,
              "Controls work again after the destination page arrives");
    }
}

void registeredTabChecks() {
    for (int tab : {0, 1}) {
        World world(apps().c_str());
        world.app.open("live");
        world.press(Key::Up);
        for (int i = 0; i < 10; ++i) world.press(Key::Left);
        for (int i = 0; i <= tab; ++i) world.press(Key::Right);
        world.press(Key::Down, 900);
        check(world.screen() == "home" &&
                  world.snapshot()["tab"].string() == (tab ? "apps" : "home") &&
                  !world.snapshot()["navigation"].boolean(),
              "Down from a registered screen opens the highlighted Home tab");
    }
}

void changingTabsChecks() {
    Json live = Json::parse(apps());
    live["layout"].set("showLive", Json::flag(true));
    live.set("sources", fields({{"channels", Json::numeric(1)}}));
    World world(apps().c_str());
    world.app.activate("tab:1");
    world.settle(900);
    world.reply("catalogue", live.dump());
    check(world.snapshot()["tab"].string() == "apps", "Inserting Live keeps Apps selected by name");
    world.press(Key::Up);
    for (int i = 0; i < 10; ++i) world.press(Key::Right);
    world.reply("catalogue", apps());
    check(world.snapshot()["tab"].string() == "apps" && world.snapshot()["nav"].number() == 3,
          "Removing Live preserves the selected page and Settings toolbar focus");
    World pending(apps().c_str());
    pending.app.activate("tab:1");
    pending.reply("catalogue", live.dump());
    pending.settle(900);
    check(pending.snapshot()["tab"].string() == "apps", "An in-flight Apps destination follows its name");
    World removed(live.dump().c_str());
    removed.app.activate("tab:1");
    removed.reply("catalogue", apps());
    removed.settle(900);
    check(removed.snapshot()["tab"].string() == "home", "A removed pending tab falls back to Inicio");
}

void titleIdentityChecks() {
    auto feed = [](const Json &items) {
        Json rows = Json::array();
        rows.add(fields({{"key", Json::text("trending")}, {"items", items}}));
        return fields({{"ok", Json::flag(true)}, {"rows", rows}}).dump();
    };
    Json pair = Json::parse(R"([{"id":"42","type":"movie","title":"Movie 42"},{"id":"42","type":"tv","title":"Series 42"}])");
    World world(apps().c_str());
    world.reply("discover", feed(pair));
    world.press(Key::Down, 700);
    world.press(Key::Right, 700);
    check(world.snapshot()["heroTitle"].string() == "Series 42", "Equal movie/TV IDs still change the hero");
    world.reply("discover", feed(pair));
    world.settle(700);
    check(world.snapshot()["column"].number() == 1 && world.snapshot()["heroTitle"].string() == "Series 42",
          "Refreshing equal movie/TV IDs preserves the selected media type");
    Json split = Json::array();
    split.add(pair[size_t(0)]);
    for (int i = 1; i < 6; ++i)
        split.add(fields({{"id", Json::text(std::to_string(i))}, {"type", Json::text("movie")},
                          {"title", Json::text("Other movie")}}));
    split.add(pair[size_t(1)]);
    World spotlight(apps().c_str());
    spotlight.reply("discover", feed(split));
    spotlight.press(Key::Down, 700);
    check(spotlight.snapshot()["rowCards"].number() == 1 &&
              spotlight.snapshot()["heroTitle"].string() == "Series 42" &&
              spotlight.motion("dots") < .01 && spotlight.motion("dot:0") < .01,
          "A movie in the carousel does not remove its TV namesake from the row");
}
}

namespace {
bool pointerOutline(World &world) {
    for (const Node &node : world.app.scene().nodes)
        if (node.value == "pointer:hover")
            return true;
    return false;
}

void pointAt(World &world, const std::string &id) {
    const Scene scene = world.app.scene();
    for (const Hit &hit : scene.hits)
        if (hit.id == id) {
            world.app.hover(hit.rect.x + hit.rect.w / 2, hit.rect.y + hit.rect.h / 2);
            return;
        }
    throw std::runtime_error("No pointer target: " + id);
}

void pointerChecks() {
    for (const char *shape : {"round", "square", "squircle", "rectangle"}) {
        Json catalogue = Json::parse(apps());
        catalogue["layout"].set("appShape", Json::text(shape));
        World world(catalogue.dump().c_str());
        world.settle(600);
        const std::string focused = world.snapshot()["focusCard"].string();
        const size_t requests = world.transport.requests.size();
        pointAt(world, "card:0:1");
        check(pointerOutline(world), "Air cursor highlights an app without clicking it");
        check(world.snapshot()["focusCard"].string() == focused &&
                  world.transport.requests.size() == requests,
              "Hover does not activate apps or move the remote's selection");
        for (const Node &node : world.app.scene().nodes)
            if (node.value == "pointer:hover")
                check(node.squircle == (std::string(shape) == "squircle") && node.ring.outer > 0,
                      "Hover follows the tile shape and paints a visible outline");
        world.app.hover(-1, -1);
        check(!pointerOutline(world), "Leaving a target clears pointer feedback");
        pointAt(world, "nav:settings");
        check(pointerOutline(world) && world.screen() == "home",
              "Hovering Settings highlights it without opening it");
        world.press(Key::Right);
        check(!pointerOutline(world), "Remote input clears the pointer highlight");
        pointAt(world, "nav:tab:1");
        check(world.snapshot()["tab"].string() == "home", "Hovering a tab never switches pages");
        world.app.open("quick");
        world.reply("quick:stats", statsJson);
        world.settle(500);
        check(!pointerOutline(world), "Opening a screen clears the previous hover target");
        const Scene drawer = world.app.scene();
        for (const Hit &hit : drawer.hits) {
            if (!hit.hoverable || hit.id.compare(0, 6, "quick:") != 0) continue;
            pointAt(world, hit.id);
            check(pointerOutline(world), "Quick settings controls support pointer feedback");
            break;
        }
        // An empty corner of the panel must not outline the entire drawer.
        world.app.hover(1910, 1070);
        check(!pointerOutline(world), "Modal background targets do not get hover outlines");
        pointAt(world, "card:0:1");
        check(!pointerOutline(world), "The drawer blocks hover on Home underneath it");
    }
    World pending(apps().c_str());
    pending.press({Key::Up, Key::Right, Key::Ok});
    pointAt(pending, "card:0:1");
    check(!pointerOutline(pending), "Leaving-page targets cannot highlight during a tab transition");
    World recents(apps().c_str());
    recents.app.open("recents");
    recents.settle(500);
    check(!hasHit(recents.app.scene(), "card:0:1"),
          "Recent apps removes pointer targets belonging to the covered Home page");
    pointAt(recents, "back");
    check(pointerOutline(recents), "Empty recent apps has a pointer-accessible way back");
    recents.app.activate("back");
    check(recents.screen() == "home", "The pointer can leave an empty recent-apps overlay");
}
}

int main() {
    try {
        shapeChecks();
        dimmingChecks();
        rowMemoryChecks();
        barBackChecks();
        tabBackChecks();
        scrollingBarChecks();
        homeResumeChecks();
        tabTransitionChecks();
        carriedAppChecks();
        registeredTabChecks();
        changingTabsChecks();
        titleIdentityChecks();
        pointerChecks();
        std::printf("%d launcher usability checks passed\n", checkCount());
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
