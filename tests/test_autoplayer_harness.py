"""Non-proprietary regression tests for the on-device campaign harness."""
import importlib.util
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "autoplayer_harness", ROOT / "tools" / "autoplayer-harness.py")
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)


FAKE_ENGINE = """import pathlib
import sys
import time
args = sys.argv
log = pathlib.Path(args[args.index('--autoplay-log') + 1])
with log.open('w', encoding='utf-8') as stream:
    stream.write('event=map_start map=MAP01 tic=1\\n')
    stream.write('event=failure detail=movement_obstruction\\n')
    stream.write('event=failure detail=player_death\\n')
    stream.flush()
    if '--simulate-timeout' in args:
        time.sleep(10)
    stream.write('event=campaign_complete map=MAP10 tic=70\\n')
    stream.flush()
"""


class HarnessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.wolf = self.root / "Wolf"
        self.wolf.mkdir()
        for name in HARNESS.CAMPAIGNS["wolf3d"][1]:
            (self.wolf / name.lower()).write_bytes(b"test placeholder")
        self.spear = self.root / "Spear"
        self.spear.mkdir()
        for name in HARNESS.CAMPAIGNS["spear"][1]:
            (self.spear / name).write_bytes(b"test placeholder")
        self.engine = self.root / "stub.py"
        self.engine.write_text(FAKE_ENGINE, encoding="utf-8")

    def test_requires_local_game_data(self):
        HARNESS.validate_data(self.wolf, HARNESS.CAMPAIGNS["wolf3d"][1])
        HARNESS.validate_data(self.spear, HARNESS.CAMPAIGNS["spear"][1])
        (self.wolf / "gamemaps.wl6").unlink()
        with self.assertRaises(ValueError):
            HARNESS.validate_data(self.wolf, HARNESS.CAMPAIGNS["wolf3d"][1])

    def test_classifies_completed_run(self):
        result = HARNESS.run_campaign(self.engine, "wolf3d", self.wolf,
                                      self.root / "reports", 3, "MAP01", 0.02)
        self.assertEqual(result["status"], "completed")
        self.assertEqual(result["map_starts"], 1)
        self.assertEqual(result["events"].get("campaign_complete"), 1)
        self.assertEqual(result["deaths"], 1)
        self.assertEqual(result["events"].get("failure"), 2)

    def test_does_not_accept_stale_success(self):
        out = self.root / "reports"
        out.mkdir()
        (out / "wolf3d-events.log").write_text("event=campaign_complete\n")
        stub = self.root / "incomplete.py"
        stub.write_text("import time\ntime.sleep(5)\n")
        result = HARNESS.run_campaign(stub, "wolf3d", self.wolf, out,
                                      0.1, "MAP01", 0.02)
        self.assertEqual(result["status"], "timeout_incomplete")
        self.assertEqual(result["events"], {})

    def test_writes_summary_for_both_campaigns(self):
        report_dir = self.root / "summary"
        code = HARNESS.main(["--engine", str(self.engine), "--campaign", "both",
                             "--wolf-data", str(self.wolf),
                             "--spear-data", str(self.spear),
                             "--out-dir", str(report_dir), "--minutes", "0.1"])
        self.assertEqual(code, 0)
        self.assertTrue((report_dir / "summary.json").is_file())


if __name__ == "__main__":
    unittest.main()
