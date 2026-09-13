#include "assets/smaa.hpp"
#include "core/file.hpp"
#include <algorithm>
#include <cassert>

int main() {
    // Compare the bytes linked into the executable with the source resources.
    // This exercises binary conversion, compilation and the exported spans.
    const auto area = space::file::read("area.bin");
    const auto search = space::file::read("search.bin");
    assert(area && search);
    assert(std::ranges::equal(*area, space::assets::smaa_area()));
    assert(std::ranges::equal(*search, space::assets::smaa_search()));
}
