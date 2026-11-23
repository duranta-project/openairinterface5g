#!/usr/bin/env python3
"""Start and monitor CELEOS, stop with --stop, or fetch retained logs with --logs."""

import argparse
from concurrent.futures import ThreadPoolExecutor, wait
from contextlib import nullcontext
import json
import math
import os
from pathlib import Path
import signal
import sys
from tempfile import NamedTemporaryFile
import time
from urllib.error import HTTPError
from urllib.request import HTTPRedirectHandler, Request, build_opener

TERMINAL = {"idle", "completed", "stopped", "force_stopped", "error", "failed"}


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None  # Do not forward the API key to a redirected host.


class Celeos:
    def __init__(self, env_path):
        cfg = {}
        for line in env_path.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                key, value = line.split("=", 1)
                cfg[key.strip()] = value.strip().strip("\"'")
        cfg.update(os.environ)
        self.key = cfg.get("X_API_KEY") or cfg.get("X-API-KEY", "")
        if not self.key or self.key == "replace-with-your-api-key":
            raise ValueError("Set X_API_KEY in .env.")
        self.url = (cfg.get("CELEOS_URL") or
                    f"{cfg.get('CELEOS_SCHEME', 'http')}://"
                    f"{cfg.get('CELEOS_IP', '127.0.0.1')}:"
                    f"{cfg.get('CELEOS_PORT', '3001')}").rstrip("/")
        for name, default in (("poll_interval", 1), ("request_timeout", 10),
                              ("start_timeout", 180), ("stop_timeout", 20)):
            value = float(cfg.get(f"CELEOS_{name.upper()}", default))
            if not math.isfinite(value) or value <= 0:
                raise ValueError(f"CELEOS_{name.upper()} must be a positive number.")
            setattr(self, name, value)
        self.log_tail = None
        self.log_warning = None
        self.clocks = {}
        for name in ("pps", "10mhz"):
            value = cfg.get(f"CELEOS_{name.upper()}", "false").lower()
            if value not in {"true", "false"}:
                raise ValueError(f"CELEOS_{name.upper()} must be true or false.")
            self.clocks[name] = value == "true"

    def request(self, method, path, body=None, timeout=None, *, download_to=None):
        data = None if body is None else json.dumps(body).encode()
        req = Request(self.url + path, data=data, method=method,
                      headers={"X-API-KEY": self.key, "Content-Type": "application/json"})
        try:
            with build_opener(NoRedirect).open(req, timeout=timeout or self.request_timeout) as response:
                if download_to is not None:
                    destination = download_to.open("wb") if isinstance(download_to, Path) else nullcontext(download_to)
                    size = 0
                    with destination as stream:
                        for chunk in iter(lambda: response.read(65536), b""):
                            stream.write(chunk)
                            size += len(chunk)
                        stream.flush()
                    length = response.headers.get("Content-Length")
                    if length is not None and size != int(length):
                        raise RuntimeError(f"Incomplete download from {path}")
                    return {}
                result = json.load(response)
        except HTTPError as exc:
            if method == "DELETE" and exc.code == 404:
                exc.close()
                return {}  # Still verify /status/state below.
            detail = exc.read().decode(errors="replace")[:1000]
            exc.close()
            raise RuntimeError(f"{method} {path}: HTTP {exc.code}: {detail}") from None
        if not isinstance(result, dict) or result.get("valid") is False:
            raise RuntimeError(f"{method} {path}: {result}")
        return result

    def status(self):
        state = self.request("GET", "/status/state")
        if state.get("state_name") not in TERMINAL | {"starting", "loading", "running", "stop_requested"}:
            raise RuntimeError(f"Unrecognized state response: {state}")
        print(f"{time.strftime('%H:%M:%S')}  {state['state_name']:14}  "
              f"{state.get('loaded-profile-name') or '-'}  {state.get('message', '')}", flush=True)
        if self.log_tail is not None:
            self.print_logs()
        return state["state_name"]

    def stop(self, pending=None):
        previous = signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            print("Stopping emulation...", flush=True)
            try:
                self.request("DELETE", "/device")
            except Exception:
                if pending is None:
                    raise
                print("Initial stop failed; retrying after startup settles.", flush=True)
            if pending is not None:
                # DELETE can arrive before PUT admission. Wait, then stop again.
                print("Waiting for the start request to settle...", flush=True)
                try:
                    pending.result()
                except Exception:
                    pass  # A cancelled start normally returns an API error.
                self.request("DELETE", "/device")
            deadline = time.monotonic() + self.stop_timeout
            while True:
                state = self.status()
                if state in TERMINAL:
                    print(f"CELEOS reports emulation inactive ({state}).", flush=True)
                    return
                if time.monotonic() >= deadline:
                    raise RuntimeError("Timed out waiting for CELEOS to stop.")
                time.sleep(self.poll_interval)
        finally:
            signal.signal(signal.SIGINT, previous)

    def print_logs(self, skip=False):
        if self.log_tail is None:
            self.log_tail = []
        try:
            entries = self.request("GET", "/status/log")["list"]
            if not isinstance(entries, list) or not all(isinstance(row, dict) for row in entries):
                raise ValueError("Invalid /status/log response")
        except Exception as exc:
            warning = str(exc).replace(self.key, "<redacted>")
            if warning != self.log_warning:
                print(f"Log warning: {warning}", file=sys.stderr, flush=True)
            self.log_warning = warning
            return  # Keep the last successful snapshot and retry next poll.
        self.log_warning = None
        overlap = min(len(self.log_tail), len(entries))
        while overlap and self.log_tail[-overlap:] != entries[:overlap]:
            overlap -= 1
        if not skip:
            for row in entries[overlap:]:
                line = f"{row.get('timestamp_utc', '')}  {row.get('message', '')}"
                print(line.replace(self.key, "<redacted>"), flush=True)
        self.log_tail = entries

    def fetch_logs(self, output=None):
        folder = output.expanduser().resolve() if output is not None else None
        if folder is not None:
            folder.mkdir(parents=True, exist_ok=True)
        failures = []
        for name, endpoint in (("debug.txt", "/debug/emulation-log-download"),
                               ("status_logs.json", "/status/log")):
            temporary = None
            try:
                if folder is not None:
                    with NamedTemporaryFile(prefix=f".{name}.", dir=folder, delete=False) as stream:
                        temporary = Path(stream.name)
                else:
                    print(f"=== {name} ===", flush=True)
                if name == "debug.txt":
                    self.request("GET", endpoint, download_to=temporary if temporary is not None else sys.stdout.buffer)
                    if folder is None:
                        print(flush=True)
                else:
                    logs = self.request("GET", endpoint)
                    if not isinstance(logs.get("list"), list):
                        raise ValueError("Invalid /status/log response")
                    text = json.dumps(logs, indent=2, ensure_ascii=False) + "\n"
                    if folder is None:
                        print(text, end="", flush=True)
                    else:
                        temporary.write_text(text, encoding="utf-8")
                if folder is not None:
                    target = folder / name
                    temporary.replace(target)
                    print(f"Saved: {target}", file=sys.stderr, flush=True)
            except KeyboardInterrupt:
                raise
            except Exception as exc:
                failures.append(f"{name}: {exc}")
            finally:
                if temporary is not None:
                    temporary.unlink(missing_ok=True)
        if failures:
            raise RuntimeError("Log retrieval incomplete: " + "; ".join(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", nargs="?", help="saved profile name")
    parser.add_argument("--stop", action="store_true", help="stop the current emulation")
    parser.add_argument("--logs", action="store_true", help="print retained debug.txt and status logs")
    parser.add_argument("--output", "-o", type=Path, metavar="DIRECTORY",
                        help="with --logs, save both logs here instead of printing them")
    parser.add_argument("--env", type=Path, default=Path(__file__).with_name(".env"))
    args = parser.parse_args()
    if sum((bool(args.profile), args.stop, args.logs)) != 1 or (args.profile and not args.profile.strip()):
        parser.error("provide exactly one of: a profile name, --stop, or --logs")
    if args.output is not None and not args.logs:
        parser.error("--output requires --logs")

    api, worker, pending = None, None, None
    cleanup, exit_code = False, 0
    try:
        api = Celeos(args.env)
        if args.logs:
            api.fetch_logs(args.output)
        elif args.stop:
            api.print_logs(skip=True)
            cleanup = True
        else:
            if api.status() not in TERMINAL:
                raise RuntimeError("CELEOS is busy. Use --stop before starting another profile.")
            api.request("POST", "/device", {"name": args.profile})
            api.print_logs(skip=True)
            print(f"Starting: {args.profile}", flush=True)
            cleanup = True
            deadline = time.monotonic() + api.start_timeout
            worker = ThreadPoolExecutor(max_workers=1)
            # Keep the pending request tracked if Ctrl+C arrives during submit.
            mask = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGINT})
            try:
                pending = worker.submit(api.request, "PUT", "/device",
                                        {"message": "start", **api.clocks}, api.start_timeout)
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, mask)
            while not wait([pending], timeout=api.poll_interval).done:
                api.status()
            pending.result()
            pending = None
            has_run = False
            while True:
                state = api.status()
                if state == "running":
                    if not has_run:
                        print(f"Running: {args.profile}. Monitoring; Ctrl+C stops emulation.", flush=True)
                    has_run = True
                elif has_run and state in TERMINAL:
                    cleanup = False
                    exit_code = int(state in {"error", "failed"})
                    break
                elif not has_run and state not in {"starting", "loading"}:
                    raise RuntimeError(f"Startup ended in state '{state}' before running.")
                if not has_run:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise RuntimeError("Timed out waiting for the running state.")
                    time.sleep(min(api.poll_interval, remaining))
                else:
                    time.sleep(api.poll_interval)
    except KeyboardInterrupt:
        if args.logs:
            print("\nLog retrieval interrupted.", file=sys.stderr, flush=True)
        else:
            print("\nInterrupted; requesting stop.", flush=True)
            cleanup = True
        exit_code = 130
    except Exception as exc:
        message = str(exc).replace(api.key, "<redacted>") if api else str(exc)
        print(f"Error: {message}", file=sys.stderr)
        exit_code = 1
    finally:
        if cleanup and api is not None:
            try:
                api.stop(pending)
            except Exception as exc:
                print(f"STOP NOT CONFIRMED: {str(exc).replace(api.key, '<redacted>')}",
                      file=sys.stderr)
                exit_code = 1
        if worker is not None:
            worker.shutdown(wait=True)
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
