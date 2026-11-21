from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parent))
import coverage as menu_coverage

class NativeCoverage(unittest.TestCase):
    def test_all_native_rows_have_real_endpoints(self):
        report = menu_coverage.validate()
        self.assertEqual(report['active_rows'], 110)
        self.assertEqual(report['implemented_rows'], 105)
        self.assertEqual(report['retired_rf_sms_rows'], ['SMS On/Off', 'SMS Mode', 'RX Filter', 'Statistics', 'Clear'])
        self.assertEqual([row['message_route'] for row in report['rows'] if row['category'] == 'SMS'],
                         ['new', 'inbox', 'sent', 'draft'])
        self.assertEqual(len(report['disabled_morse_rows']),9)
    def test_missing_implemented_function_fails_build_audit(self):
        with self.assertRaisesRegex(ValueError,'missing actions'):
            menu_coverage.validate(action_ids={'channels'})
    def test_absent_gps_module_fails_build_audit(self):
        with self.assertRaisesRegex(ValueError,'missing modules'):
            menu_coverage.validate(module_ids={'APRS', '.APRS STORE', 'APRS MSG', 'APRS SEND'})

if __name__=='__main__': unittest.main()
