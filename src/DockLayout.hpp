// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>

namespace DockLayout
{

struct Sizes
{
	int icon;
	int padding;
	int spacing;
	int margin;
};

// All dimensions are logical pixels. Keep at least one pixel per button.
inline Sizes fit(int length, int thickness, int count, int preferredIcon, int preferredSpacing, int preferredMargin = 0)
{
	count = std::max(1, count);
	// Preserve the requested per-side margin before spending space on icons.
	// Only reduce it when the allocation cannot fit margins plus one-pixel icons.
	const int margin = std::min(std::max(0, preferredMargin), std::max(0, (length / count - 1) / 2));
	length -= 2 * margin * count;
	const long long natural = static_cast<long long>(count) * (preferredIcon + 8)
		+ static_cast<long long>(count - 1) * preferredSpacing;
	const int spacing = count > 1 ? static_cast<int>(std::min<long long>(preferredSpacing,
		static_cast<long long>(preferredSpacing) * length / std::max(1LL, natural))) : 0;
	const int side = std::max(1, std::min(thickness, (length - (count - 1) * spacing) / count));
	const int padding = std::min(4, std::min((side - 1) / 2, 4 * side / std::max(1, preferredIcon + 8)));
	return {std::max(1, std::min(preferredIcon, side - 2 * padding)), padding, spacing, margin};
}

} // namespace DockLayout
