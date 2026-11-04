"""Check the reconstructed TX selector against its decoded case table."""

import unittest

from aprs_tx_route_model import route_aprs_tx


class AprsTxRouteModelTests(unittest.TestCase):
    def test_lock_modes_refuse_the_other_current_side(self):
        for dual_watch in (False, True):
            self.assertFalse(route_aprs_tx(0, "B", dual_watch).allowed)
            self.assertFalse(route_aprs_tx(1, "A", dual_watch).allowed)
            self.assertEqual(route_aprs_tx(0, "A", dual_watch).channel_setup, "A")
            self.assertEqual(route_aprs_tx(1, "B", dual_watch).channel_setup, "B")

    def test_auto_modes_temporarily_select_the_requested_side(self):
        for dual_watch in (False, True):
            a = route_aprs_tx(2, "B", dual_watch)
            b = route_aprs_tx(3, "A", dual_watch)
            self.assertEqual((a.channel_setup, b.channel_setup), ("A", "B"))
            self.assertTrue(a.temporarily_changes_current_side)
            self.assertTrue(b.temporarily_changes_current_side)

    def test_both_mode_keeps_the_current_side(self):
        for dual_watch in (False, True):
            for current in ("A", "B"):
                route = route_aprs_tx(4, current, dual_watch)
                self.assertTrue(route.allowed)
                self.assertEqual(route.channel_setup, current)
                self.assertFalse(route.temporarily_changes_current_side)


if __name__ == "__main__":
    unittest.main()
