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
    for (uint32_t delay : {0u, 100u, 800u})
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
}

int main() {
    try {
        shapeChecks();
        dimmingChecks();
        rowMemoryChecks();
        barBackChecks();
        tabBackChecks();
        scrollingBarChecks();
        carriedAppChecks();
        std::printf("%d launcher usability checks passed\n", checkCount());
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
