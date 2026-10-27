// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/DockLayout.hpp"
#include <cassert>
#include <iostream>

int main()
{
    // Every icon, its padding, and every gap must fit the allocation, including
    // pathological overflow down to one logical pixel per application.
    for (int count = 1; count <= 150; ++count)
        for (int length = count; length <= 2048; length += 7)
            for (int thickness : {16, 24, 32, 48, 64})
            {
                const auto s = DockLayout::fit(length, thickness, count, 32, thickness / 10);
                assert(s.icon >= 1 && s.icon <= 32);
                assert(s.padding >= 0 && s.spacing >= 0);
                assert(count * (s.icon + 2 * s.padding) + (count - 1) * s.spacing <= length);
                assert(s.icon + 2 * s.padding <= thickness);
            }
    const auto full = DockLayout::fit(436, 48, 10, 32, 4);
    const auto tight = DockLayout::fit(200, 48, 10, 32, 4);
    const auto restored = DockLayout::fit(436, 48, 10, 32, 4);
    assert(full.icon == 32 && tight.icon < full.icon && restored.icon == 32);
    const auto empty = DockLayout::fit(1, 1, 0, 32, 4);
    assert(empty.icon == 1 && empty.padding == 0 && empty.spacing == 0);
    for (int margin : {0, 1, 5, 16, 32})
        for (int count = 1; count <= 100; ++count)
            for (int length = count; length <= 1600; length += 7)
            {
                const auto s = DockLayout::fit(length, 61, count, 48, 6, margin);
                assert(count * (s.icon + 2 * s.padding + 2 * s.margin)
                    + (count - 1) * s.spacing <= length);
                assert(s.margin >= 0 && s.margin <= margin);
                if (length >= count * (2 * margin + 1)) assert(s.margin == margin);
            }
    std::cout << "Dock layout tests passed\n";
}
