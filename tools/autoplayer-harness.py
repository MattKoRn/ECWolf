#!/usr/bin/env python3
"""Run ECWolf's F12 autoplayer against locally licensed Wolf3D/Spear data.

No game data is bundled. This runs the real graphical engine; a display and
functional ECWolf installation are required on the target machine.
"""
import argparse
import collections
import concurrent.futures
import json
import pathlib
import subprocess
import sys
import time


CAMPAIGNS = {
    "wolf3d": ("WL6", ("GAMEMAPS.WL6", "MAPHEAD.WL6", "VSWAP.WL6")),
    "spear": ("SOD", ("GAMEMAPS.SOD", "MAPHEAD.SOD", "VSWAP.SOD")),
}


def validate_data(directory, required):
    if not directory or not directory.is_dir():
        raise ValueError("Game data directory does not exist: {}".format(directory))
    names = {p.name.upper() for p in directory.iterdir() if p.is_file()}
    missing = [name for name in required if name not in names]
    if missing:
        raise ValueError("Missing licensed game data in {}: {}".format(
            directory, ", ".join(missing)))


def read_events(path):
    if not path.exists():
        return []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    return [part[6:] for line in lines for part in line.split()
            if part.startswith("event=")]


def run_campaign(engine, campaign, data_dir, out_dir, seconds, start_map,
                 poll_seconds=1.0):
    ext, required = CAMPAIGNS[campaign]
    validate_data(data_dir, required)
    out_dir.mkdir(parents=True, exist_ok=True)
    telemetry = out_dir / (campaign + "-events.log")
    output = out_dir / (campaign + "-engine.log")
    # Prevent stale logs from being mistaken for a successful new run.
    if telemetry.exists():
        telemetry.unlink()
    cmd = ([sys.executable, str(engine)] if engine.suffix.lower() == ".py"
           else [str(engine)])
    cmd += ["--data", ext, "--tedlevel", start_map, "--nowait",
            "--autoplay", "--autoplay-log", str(telemetry)]
    start = time.monotonic()
    completed = False
    timed_out = False
    exit_code = None
    with output.open("w", encoding="utf-8") as capture:
        proc = subprocess.Popen(cmd, cwd=str(data_dir), stdout=capture,
                                stderr=subprocess.STDOUT,
                                stdin=subprocess.DEVNULL)
        try:
            while True:
                exit_code = proc.poll()
                events = read_events(telemetry)
                if "campaign_complete" in events and "map_start" in events:
                    completed = True
                    break
                if exit_code is not None:
                    break
                if time.monotonic() - start >= seconds:
                    timed_out = True
                    break
                time.sleep(poll_seconds)
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
            exit_code = proc.returncode
    events = read_events(telemetry)
    counts = dict(collections.Counter(events))
    event_lines = (telemetry.read_text(encoding="utf-8", errors="replace")
                   .splitlines() if telemetry.exists() else [])
    death_count = sum(1 for line in event_lines if
                      "event=failure" in line.split() and
                      "detail=player_death" in line.split())
    if completed:
        status = "completed"
    elif timed_out:
        status = "timeout_incomplete"
    else:
        status = "exited_incomplete"
    return {
        "campaign": campaign,
        "status": status,
        "seconds": round(time.monotonic() - start, 2),
        "engine_exit_code": exit_code,
        "map_starts": counts.get("map_start", 0),
        "deaths": death_count,
        "events": counts,
        "event_log": str(telemetry),
        "engine_log": str(output),
        "command": cmd,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=pathlib.Path,
                        help="Installed ecwolf executable or testing stub")
    parser.add_argument("--campaign", choices=("wolf3d", "spear", "both"),
                        default="both")
    parser.add_argument("--wolf-data", type=pathlib.Path,
                        help="Directory with legally owned WL6 data files")
    parser.add_argument("--spear-data", type=pathlib.Path,
                        help="Directory with legally owned SOD data files")
    parser.add_argument("--out-dir", type=pathlib.Path,
                        default=pathlib.Path("build/autoplayer-playthroughs"))
    parser.add_argument("--minutes", type=float, default=20.0,
                        help="Runtime budget per campaign, not a level timeout")
    parser.add_argument("--start-map", default="MAP01")
    parser.add_argument("--sequential", action="store_true",
                        help="Run both campaigns one after another rather than concurrently")
    args = parser.parse_args(argv)
    if args.minutes <= 0:
        parser.error("--minutes must be positive")
    engine = args.engine.expanduser().resolve()
    if not engine.is_file():
        parser.error("Engine executable does not exist: {}".format(engine))
    output = args.out_dir.expanduser().resolve()
    campaigns = ("wolf3d", "spear") if args.campaign == "both" else (args.campaign,)
    sources = {"wolf3d": args.wolf_data, "spear": args.spear_data}
    try:
        for campaign in campaigns:
            directory = sources[campaign]
            validate_data(directory.expanduser().resolve() if directory else None,
                          CAMPAIGNS[campaign][1])
    except ValueError as exc:
        parser.error(str(exc))

    # Each engine gets its own game-data working directory and telemetry
    # files. For "both", launch both processes together, then keep summary
    # ordering deterministic regardless of which campaign finishes first.
    mode = "parallel" if len(campaigns) == 2 and not args.sequential else "sequential"

    def run_one(campaign):
        data_dir = sources[campaign].expanduser().resolve()
        try:
            return run_campaign(engine, campaign, data_dir, output,
                                args.minutes * 60.0, args.start_map)
        except OSError as exc:
            return {"campaign": campaign, "status": "launch_failed",
                    "error": str(exc)}

    results_by_campaign = {}
    if mode == "parallel":
        print("Running Wolfenstein 3D and Spear of Destiny concurrently",
              flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures = {pool.submit(run_one, name): name for name in campaigns}
            for future in concurrent.futures.as_completed(futures):
                name = futures[future]
                results_by_campaign[name] = future.result()
                print("{}: {}".format(name,
                      results_by_campaign[name]["status"]), flush=True)
    else:
        for name in campaigns:
            print("Running {} using local data at {}".format(
                name, sources[name]), flush=True)
            results_by_campaign[name] = run_one(name)
            print("{}: {}".format(name,
                  results_by_campaign[name]["status"]), flush=True)
    results = [results_by_campaign[name] for name in campaigns]
    output.mkdir(parents=True, exist_ok=True)
    report = output / "summary.json"
    report.write_text(json.dumps({"execution_mode": mode, "results": results},
                                 indent=2) + "\n", encoding="utf-8")
    print("Report:", report)
    return 0 if all(x["status"] == "completed" for x in results) else 2


if __name__ == "__main__":
    sys.exit(main())
