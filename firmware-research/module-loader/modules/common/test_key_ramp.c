#include <assert.h>
#include <stdio.h>
#include "key_ramp.h"

int main(void)
{
    struct h9_key_ramp keys = {0};
    assert(!h9_key_ramp(&keys, H9_KEY_DOWN, 10, 1));
    assert(h9_key_ramp(&keys, H9_KEY_DOWN, 12, 1) == H9_KEY_DOWN);
    assert(!h9_key_ramp(&keys, H9_KEY_DOWN, 61, 1));
    assert(!h9_key_ramp(&keys, H9_KEY_NONE, 62, 1));
    assert(!h9_key_ramp(&keys, H9_KEY_NONE, 64, 1));

    unsigned first = 0, medium = 0, fast = 0;
    for (uint32_t time = 0; time < 280; ++time) {
        unsigned event = h9_key_ramp(&keys, H9_KEY_UP, time + 100, 1);
        if (!event) continue;
        assert(event == H9_KEY_UP);
        if (time < 100) ++first;
        else if (time < 180) ++medium;
        else ++fast;
    }
    assert(first == 5 && medium >= 8 && fast >= 20);

    /* Release and a new direction get a fresh first-repeat delay. */
    assert(!h9_key_ramp(&keys, H9_KEY_NONE, 400, 1));
    assert(!h9_key_ramp(&keys, H9_KEY_NONE, 402, 1));
    assert(!h9_key_ramp(&keys, H9_KEY_DOWN, 403, 1));
    assert(h9_key_ramp(&keys, H9_KEY_DOWN, 405, 1) == H9_KEY_DOWN);
    assert(!h9_key_ramp(&keys, H9_KEY_DOWN, 454, 1));
    assert(h9_key_ramp(&keys, H9_KEY_DOWN, 455, 1) == H9_KEY_DOWN);

    keys = (struct h9_key_ramp){ .sample = H9_KEY_UP, .stable = H9_KEY_UP };
    assert(!h9_key_ramp(&keys, H9_KEY_UP, 500, 1));
    assert(!h9_key_ramp(&keys, H9_KEY_MENU, 501, 1));
    assert(h9_key_ramp(&keys, H9_KEY_MENU, 503, 1) == H9_KEY_MENU);
    assert(!h9_key_ramp(&keys, H9_KEY_MENU, 600, 1));

    keys = (struct h9_key_ramp){0};
    assert(!h9_key_ramp(&keys, H9_KEY_UP, UINT32_MAX - 5, 0));
    assert(h9_key_ramp(&keys, H9_KEY_UP, UINT32_MAX - 4, 0) == H9_KEY_UP);
    assert(!h9_key_ramp(&keys, H9_KEY_UP, 44, 1));
    assert(!h9_key_ramp(&keys, H9_KEY_UP, 45, 0));
    assert(h9_key_ramp(&keys, H9_KEY_UP, 45, 1) == H9_KEY_UP);
    puts("Key ramp: tap, three speeds, release, held-on-entry, and timer wrap passed.");
}
