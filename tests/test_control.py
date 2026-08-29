import importlib.util
import json
import os
import tempfile
import threading
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
LOADER = importlib.machinery.SourceFileLoader("wallpaper_control", str(ROOT / "tools/orbit-wallpaper-control"))
SPEC = importlib.util.spec_from_loader("wallpaper_control", LOADER)
CONTROL = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(CONTROL)


class ControlApiTests(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        root = Path(self.tempdir.name)
        self.config = root / "config"
        self.config.write_text(
            "# preserved\n"
            "ORBIT_WALLPAPER_SPEED=1.0\n"
            "unrelated=value\n",
            encoding="utf-8",
        )
        CONTROL.CONFIG_PATH = self.config
        CONTROL.CONFIG_DIR = root
        CONTROL.SHADER_DIR = root / "shaders"
        CONTROL.STATE_PATH = root / "state.json"
        CONTROL.CONTROL_PATH = root / "control"
        CONTROL.EVENTS_PATH = root / "events"

    def tearDown(self):
        self.tempdir.cleanup()

    def test_config_set_preserves_unrelated_lines_and_stages(self):
        payload = CONTROL.stage_config({"ORBIT_WALLPAPER_SPEED": "1.5"})
        self.assertTrue(payload["ok"])
        self.assertIn("# preserved", self.config.read_text())
        self.assertIn("unrelated=value", self.config.read_text())
        self.assertIn("ORBIT_WALLPAPER_SPEED=1.0", self.config.read_text())
        self.assertTrue(payload["restart_required"])

    def test_scale_between_monitors_accepts_enabled_and_disabled_values(self):
        enabled = CONTROL.stage_config({"ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS": "1"})
        self.assertTrue(enabled["ok"])
        self.assertIn("ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS", enabled["pending_changes"])
        self.assertEqual(
            CONTROL.load_json(CONTROL.STATE_PATH, {})["pending"]["ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS"],
            "1",
        )

        disabled = CONTROL.stage_config({"ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS": "false"})
        self.assertTrue(disabled["ok"])
        self.assertEqual(
            CONTROL.load_json(CONTROL.STATE_PATH, {})["pending"]["ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS"],
            "false",
        )

    def test_scale_between_monitors_rejects_invalid_boolean(self):
        with self.assertRaises(CONTROL.APIError) as raised:
            CONTROL.stage_config({"ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS": "maybe"})
        self.assertEqual(raised.exception.code, "invalid_config")

    def test_scale_between_monitors_is_omitted_without_explicit_config(self):
        payload = CONTROL.config_get()
        self.assertNotIn("ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS", payload["settings"])
        self.assertEqual(CONTROL.CONFIG_SPECS["ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS"]["kind"], "bool")

    def test_malformed_config_is_not_overwritten(self):
        original = "ORBIT_WALLPAPER_SPEED=1.0\nmalformed\n"
        self.config.write_text(original)
        with self.assertRaises(CONTROL.APIError) as raised:
            CONTROL.stage_config({"ORBIT_WALLPAPER_SPEED": "1.5"})
        self.assertEqual(raised.exception.code, "invalid_config")
        self.assertEqual(self.config.read_text(), original)

    def test_missing_fifo_is_structured_error(self):
        with self.assertRaises(CONTROL.APIError) as raised:
            CONTROL.fifo_command("intro", "test-id", CONTROL.INTRO_TIMEOUT)
        self.assertEqual(raised.exception.code, "fifo_unavailable")

    def test_fifo_write_timeout_is_structured_error(self):
        os.mkfifo(self.config.parent / "control")
        os.mkfifo(self.config.parent / "events")
        CONTROL.CONTROL_PATH = self.config.parent / "control"
        CONTROL.EVENTS_PATH = self.config.parent / "events"
        control_reader = os.open(CONTROL.CONTROL_PATH, os.O_RDONLY | os.O_NONBLOCK)
        try:
            with mock.patch.object(CONTROL.os, "write", side_effect=BlockingIOError), mock.patch.object(CONTROL.select, "select", return_value=([], [], [])):
                with self.assertRaises(CONTROL.APIError) as raised:
                    CONTROL.fifo_command("intro", "test-id", CONTROL.INTRO_TIMEOUT)
            self.assertEqual(raised.exception.code, "fifo_write_timeout")
        finally:
            os.close(control_reader)

    def test_status_ignores_unknown_status_fields(self):
        with mock.patch.object(CONTROL, "service_properties", return_value={
            "ActiveState": "active", "SubState": "running", "MainPID": "42",
            "Environment": "ORBIT_WALLPAPER_FOLLOW_SYSTEM_PALETTE=1 ORBIT_WALLPAPER_PALETTE_FILE=/tmp/p.lua",
        }), mock.patch.object(CONTROL, "status_values", return_value={
            "readiness": "ready", "surfaces": "2/2", "animation": "normal",
            "future_field": "ignored",
        }), mock.patch.object(CONTROL, "config_values", return_value={"ORBIT_WALLPAPER_SHADER": "wave.frag"}), mock.patch.object(CONTROL, "command_state", return_value={"pending": {}}), mock.patch.object(CONTROL, "renderer_version", return_value="test"):
            payload = CONTROL.status_command()
        self.assertEqual(payload["api_version"], 1)
        self.assertEqual(payload["outputs"], {"count": 2, "active_surfaces": 2})
        self.assertNotIn("future_field", json.dumps(payload))

    def test_status_handles_missing_renderer(self):
        with mock.patch.object(CONTROL, "service_properties", return_value={}), mock.patch.object(CONTROL, "status_values", return_value={}), mock.patch.object(CONTROL, "config_values", return_value={}), mock.patch.object(CONTROL, "command_state", return_value={}), mock.patch.object(CONTROL, "renderer_version", return_value=None):
            payload = CONTROL.status_command()
        self.assertEqual(payload["readiness"], "unavailable")
        self.assertEqual(payload["renderer"]["state"], "unknown")

    def test_shader_apply_rolls_back_configuration(self):
        original = self.config.read_text()
        (self.config.parent / "shaders").mkdir()
        (self.config.parent / "shaders" / "safe.frag").write_text("void main() {}")

        with mock.patch.object(CONTROL, "run_helper", return_value={
            "ok": True, "filename": "safe.frag", "name": "Safe", "installed": True,
        }), mock.patch.object(
            CONTROL, "apply_pending_config", side_effect=CONTROL.APIError("restart_failed", "test failure")
        ), mock.patch.object(CONTROL, "restart_renderer", return_value=CONTROL.result(restarted=True)) as restart:
            with self.assertRaises(CONTROL.APIError) as raised:
                CONTROL.shader_apply("safe-id", False)
        self.assertEqual(raised.exception.code, "shader_apply_failed")
        self.assertEqual(self.config.read_text(), original)
        restart.assert_not_called()

    def test_shader_apply_installs_applies_and_verifies_active_shader(self):
        self.config.write_text("ORBIT_WALLPAPER_SHADER=old.frag\n")
        (self.config.parent / "shaders").mkdir()
        (self.config.parent / "shaders" / "new.frag").write_text("void main() {}")

        def restart(*, replay_intro=False, clear_pending=True):
            return {"status": {"shader": {"active": "new.frag"}}}

        with mock.patch.object(CONTROL, "run_helper", return_value={
            "ok": True, "filename": "new.frag", "name": "New", "installed": True,
        }), mock.patch.object(CONTROL, "restart_renderer", side_effect=restart), mock.patch.object(
            CONTROL, "fifo_command", return_value={"ok": True}
        ):
            payload = CONTROL.shader_apply("new-id", True)

        self.assertTrue(payload["applied"])
        self.assertEqual(payload["shader"]["filename"], "new.frag")
        self.assertIn("ORBIT_WALLPAPER_SHADER=new.frag", self.config.read_text())

    def test_shader_apply_rejects_duplicate_while_install_is_delayed(self):
        (self.config.parent / "shaders").mkdir()
        (self.config.parent / "shaders" / "new.frag").write_text("void main() {}")
        started = threading.Event()
        release = threading.Event()

        def delayed_install(_arguments):
            started.set()
            release.wait(2)
            return {"ok": True, "filename": "new.frag", "name": "New", "installed": True}

        with mock.patch.object(CONTROL, "run_helper", side_effect=delayed_install), mock.patch.object(
            CONTROL,
            "apply_pending_config",
            return_value={"restart": {"status": {"shader": {"active": "new.frag"}}}},
        ):
            worker = threading.Thread(target=CONTROL.shader_apply, args=("new-id", True))
            worker.start()
            self.assertTrue(started.wait(1))
            with self.assertRaises(CONTROL.APIError) as raised:
                CONTROL.shader_apply("new-id", True)
            self.assertEqual(raised.exception.code, "shader_apply_busy")
            release.set()
            worker.join(2)

    def test_shader_install_apply_qml_requests_intro_and_blocks_refresh_during_apply(self):
        qml = (ROOT / "settings" / "WallpaperSettings.qml").read_text(encoding="utf-8")
        self.assertIn('String(item.id),\n                    "--intro"', qml)
        self.assertIn("&& !shaderInstallProcess.running", qml)
        self.assertIn("enabled: !shaderInstallProcess.running", qml)

    def test_multi_monitor_setting_qml_uses_canonical_key_and_protects_shader(self):
        qml = (ROOT / "settings" / "WallpaperSettings.qml").read_text(encoding="utf-8")
        self.assertIn('text: "Scale between multiple monitors"', qml)
        self.assertIn('text: "Treat connected displays as one continuous canvas."', qml)
        self.assertIn('"ORBIT_WALLPAPER_SCALE_BETWEEN_MONITORS", scaleBetweenMonitors ? "1" : "0"', qml)
        self.assertIn('if (shaderSelectionDirty)', qml)
        self.assertIn('command.push("ORBIT_WALLPAPER_SHADER"', qml)

    def test_inspect_normalizes_helper_payload(self):
        with mock.patch.object(CONTROL, "run_helper", return_value={
            "ok": True, "shaders": [{"id": "one", "supported": True}],
        }):
            payload = CONTROL.shader_inspect("one")
        self.assertEqual(payload["api_version"], 1)
        self.assertEqual(payload["shader"]["id"], "one")

    def test_catalogue_normalizes_helper_payload(self):
        with mock.patch.object(CONTROL, "run_helper", return_value={
            "ok": True, "source": "catalogue", "shaders": [],
        }):
            payload = CONTROL.shader_catalogue(False)
        self.assertEqual(payload["api_version"], 1)
        self.assertEqual(payload["source"], "catalogue")

    def test_shader_cycle_wraps_and_uses_safe_apply(self):
        with mock.patch.object(CONTROL, "shader_list_installed", return_value={"shaders": ["one.frag", "two.frag"]}), mock.patch.object(
            CONTROL, "read_config", return_value=("", {"ORBIT_WALLPAPER_SHADER": "one.frag"})
        ), mock.patch.object(CONTROL, "shader_apply", return_value={"ok": True}) as apply:
            CONTROL.shader_cycle(-1)
        apply.assert_called_once_with("two.frag", replay_intro=False)

    def test_shader_cycle_commands_parse(self):
        self.assertEqual(CONTROL.parser().parse_args(["shader", "previous"]).command, "shader-previous")
        self.assertEqual(CONTROL.parser().parse_args(["shader", "next"]).command, "shader-next")

    def test_animation_timeout_follows_configured_duration(self):
        self.config.write_text("ORBIT_WALLPAPER_INTRO_DURATION=9.0\n")
        self.assertEqual(CONTROL.command_timeout("intro"), 11.0)

    def test_config_apply_exits_restarts_and_replays_intro(self):
        CONTROL.stage_config({"ORBIT_WALLPAPER_SPEED": "1.5"})
        calls = []

        def fifo(command, identifier, timeout):
            calls.append((command, timeout, self.config.read_text()))
            return {"ok": True, "command": command}

        with mock.patch.object(CONTROL, "fifo_command", side_effect=fifo), mock.patch.object(
            CONTROL, "restart_renderer", return_value={"ok": True}
        ):
            payload = CONTROL.apply_pending_config()

        self.assertTrue(payload["applied"])
        self.assertEqual([command for command, _, _ in calls], ["exit", "intro"])
        self.assertIn("ORBIT_WALLPAPER_SPEED=1.0", calls[0][2])
        self.assertIn("ORBIT_WALLPAPER_SPEED=1.5", calls[1][2])
        self.assertIn("ORBIT_WALLPAPER_SPEED=1.5", self.config.read_text())
        self.assertEqual(CONTROL.command_state().get("pending"), {})

    def test_config_apply_persists_numeric_and_boolean_values(self):
        CONTROL.stage_config({
            "ORBIT_WALLPAPER_SPEED": "1.75",
            "ORBIT_WALLPAPER_RESOURCE_GOVERNOR": "1",
        })
        with mock.patch.object(CONTROL, "fifo_command", return_value={"ok": True}), mock.patch.object(
            CONTROL, "restart_renderer", return_value={"ok": True}
        ):
            payload = CONTROL.apply_pending_config()

        self.assertTrue(payload["applied"])
        persisted = self.config.read_text()
        self.assertIn("ORBIT_WALLPAPER_SPEED=1.75", persisted)
        self.assertIn("ORBIT_WALLPAPER_RESOURCE_GOVERNOR=1", persisted)
        self.assertEqual(CONTROL.config_get()["settings"]["ORBIT_WALLPAPER_SPEED"]["value"], "1.75")
        self.assertEqual(CONTROL.config_get()["settings"]["ORBIT_WALLPAPER_RESOURCE_GOVERNOR"]["value"], "1")

    def test_config_apply_restores_previous_config_after_failure(self):
        original = self.config.read_text()
        CONTROL.stage_config({"ORBIT_WALLPAPER_SPEED": "1.5"})
        with mock.patch.object(CONTROL, "fifo_command", return_value={"ok": True}), mock.patch.object(
            CONTROL,
            "restart_renderer",
            side_effect=[CONTROL.APIError("restart_failed", "test failure"), {"ok": True}],
        ):
            with self.assertRaises(CONTROL.APIError) as raised:
                CONTROL.apply_pending_config()
        self.assertEqual(raised.exception.code, "apply_failed")
        self.assertEqual(self.config.read_text(), original)
        self.assertEqual(CONTROL.command_state().get("pending"), {})


if __name__ == "__main__":
    unittest.main()
