#include "soft_raster.hpp"
#include "graphics/debug.hpp"
#include <cstdio>
#include <stdexcept>

using namespace nativetv;
namespace {
void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
struct Host : BatchHost {
    bool textureSize(TextureId, int &w, int &h) const override { w = h = 8; return true; }
    bool textRun(RunHandle, TextRun &) const override { return false; }
};
soft::Framebuffer render(Node node, bool noCore, const soft::TextureTable &textures) {
    Host host;
    Batcher batch(host);
    debugSkip().noCore = noCore;
    batch.begin(160, 160, 1);
    batch.add(node, node.kind == Node::Kind::Image ? 1 : 0);
    check(batch.batchCount() <= 2, "Squircle adds unexpected draw batches");
    soft::Framebuffer fb(160, 160, {0, 0, 0, 1});
    soft::draw(fb, batch, textures, 1);
    debugSkip().noCore = false;
    return fb;
}
}
int main() {
    try {
        soft::TextureTable textures;
        soft::Image white;
        white.width = white.height = 8;
        white.pixels.assign(8 * 8 * 4, 255);
        textures[1] = white;
        Node node;
        node.rect = {24, 24, 112, 112};
        node.color = {1, 1, 1, 1};
        node.radius = 12;
        node.squircle = true;
        auto fill = render(node, false, textures);
        auto fullField = render(node, true, textures);
        check(fill.byte(80, 80, 0) == 255, "The centre of a squircle must not become a hole");
        for (int y = 0; y < 160; ++y) for (int x = 0; x < 160; ++x) {
            check(std::abs(int(fill.byte(x, y, 0)) - int(fullField.byte(x, y, 0))) <= 1,
                  "Flat-core optimization changes the squircle boundary");
            float nx = std::fabs((x + .5f - 80) / 56), ny = std::fabs((y + .5f - 80) / 56);
            float f = nx*nx*nx*nx + ny*ny*ny*ny - 1;
            if (f < -.15f) check(fill.byte(x, y, 0) == 255, "Interior differs from the exponent-4 curve");
            if (f > .15f) check(fill.byte(x, y, 0) == 0, "Exterior differs from the exponent-4 curve");
        }
        node.kind = Node::Kind::Image;
        auto image = render(node, false, textures);
        check(fill.rgb == image.rgb, "Artwork mask and fill use different squircle curves");
        node.kind = Node::Kind::Rectangle;
        node.gradient = Gradient::between(0, {1, 1, 1, 1}, {1, 1, 1, 1});
        auto gradient = render(node, false, textures);
        int gradientError = 0;
        for (int y = 0; y < 160; ++y) for (int x = 0; x < 160; ++x)
            gradientError = std::max(gradientError, std::abs(int(fill.byte(x, y, 0)) - int(gradient.byte(x, y, 0))));
        std::printf("Maximum gradient mask error: %d/255\n", gradientError);
        check(gradientError <= 1, "Gradient background uses a different squircle curve");
        node.gradient.stops = 0;
        node.color.a = 0;
        node.ring = {2, 2, {1, 1, 1, 1}};
        auto ring = render(node, false, textures);
        check(ring.byte(80, 24, 0) > 200 && ring.byte(127, 127, 0) > 200 && ring.byte(80, 80, 0) == 0,
              "The focus ring must follow the boundary without filling the centre");
        node.ring = Ring();
        node.color.a = 1;
        node.squircle = false;
        auto rounded = render(node, false, textures);
        check(rounded.rgb != fill.rgb, "Rounded square and squircle must remain distinct");
        check(sizeof(Vertex) == 68, "Squircle must not increase vertex upload size");
        std::puts("Squircle geometry, optimized core, image/gradient masks, focus ring and vertex budget passed");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
