#include "imux/core/Engine.hpp"
int main() {
    imux::Engine engine({});
    if (!engine.initialize()) return 1;
    engine.run();
    engine.shutdown();
    return 0;
}
