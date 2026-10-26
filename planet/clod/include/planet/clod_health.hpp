#pragma once

namespace planet {

// Coverage observations, not pixel visibility or proof of a cache flush. Counters survive resets.
struct ClodHealth {
    unsigned rollback_quadrants = 0, rollback_levels = 0;
    unsigned selection_rollbacks = 0, balance_rollbacks = 0, missing_previous = 0;
    unsigned rollback_events = 0, last_frame = 0, last_quadrants = 0, last_levels = 0;
    unsigned last_selection = 0, last_balance = 0, last_missing = 0;
    unsigned last_key = 0, last_from = 0, last_to = 0;
    unsigned invalidations = 0, timeouts = 0, releases = 0, stale_completions = 0;
    void clear_frame() {
        rollback_quadrants = rollback_levels = 0;
        selection_rollbacks = balance_rollbacks = missing_previous = 0;
    }
};

} // namespace planet
