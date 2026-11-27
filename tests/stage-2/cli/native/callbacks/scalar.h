/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

/* Call the same typed callback twice and pass its last result to the caller. */
static inline int fixture_invoke(int (*callback)(int))
{
	const int first = callback(6);
	return first == 7 ? callback(41) : -99;
}
