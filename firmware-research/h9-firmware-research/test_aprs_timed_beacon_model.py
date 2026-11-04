"""Check the reconstructed beacon boundaries against the decoded branch path."""

import unittest

from aprs_timed_beacon_model import BeaconTick, tick_timed_beacon


class TimedBeaconModelTests(unittest.TestCase):
    def test_disabled_resets_counter_and_zero_interval_preserves_it(self):
        self.assertEqual(tick_timed_beacon(False, 60, 90, 0, 1), BeaconTick(0, False, False))
        self.assertEqual(tick_timed_beacon(True, 0, 90, 0, 1), BeaconTick(90, False, False))

    def test_selector_is_called_at_interval_times_100(self):
        self.assertEqual(tick_timed_beacon(True, 60, 5998, 0, 0),
                         BeaconTick(5999, False, False))
        self.assertEqual(tick_timed_beacon(True, 60, 5999, 0, 0),
                         BeaconTick(0, True, True))

    def test_priority_guard_can_skip_selector_but_resets_counter(self):
        self.assertEqual(tick_timed_beacon(True, 1, 99, 1, 0),
                         BeaconTick(0, False, False))
        self.assertEqual(tick_timed_beacon(True, 1, 99, 1, 1),
                         BeaconTick(0, True, False))


if __name__ == "__main__":
    unittest.main()
