#ifndef WRIST_WAKE_H
#define WRIST_WAKE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool known;
    bool up;
    bool candidate_up;
    uint8_t consecutive;
} WristWake;

/* Angles in radians, sensor +Z facing the viewer. Three consecutive valid
 * samples confirm a transition; the initial pose alone must not wake. */
static inline bool WristWake_Update(WristWake *state, bool valid, float roll, float pitch)
{
    if (!valid) { state->consecutive = 0; return false; }
    float threshold = state->up ? 0.85f : 0.60f;
    bool up = roll > -threshold && roll < threshold &&
              pitch > -threshold && pitch < threshold;
    if (state->known && up == state->up) { state->consecutive = 0; return false; }
    if (state->consecutive == 0 || state->candidate_up != up) {
        state->candidate_up = up;
        state->consecutive = 1;
    } else ++state->consecutive;
    if (state->consecutive < 3) return false;
    bool wake = state->known && up;
    state->known = true;
    state->up = up;
    state->consecutive = 0;
    return wake;
}
#endif
