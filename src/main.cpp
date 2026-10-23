#include "app/shell.hpp"
#include <NeForce/tui/application.hpp>

int main(const int argc, char** argv) {
    using namespace neforce;

    string theme;
    for (int i = 1; i < argc; ++i) {
        const string_view arg{argv[i]};
        if (arg == "--theme" && i + 1 < argc) {
            theme = string{argv[++i]};
        }
    }
    return tui::application().with_component<newave::shell>(move(theme)).with_fps(30).with_title("NexusWave").run();
}
