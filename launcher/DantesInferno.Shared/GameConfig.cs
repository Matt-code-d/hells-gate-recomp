using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace DantesInferno
{
    public class GameConfig
    {
        private readonly Dictionary<string, string> _values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        private readonly string _path;

        public GameConfig(string path)
        {
            _path = path ?? string.Empty;
        }

        public static GameConfig Load(string path)
        {
            var cfg = new GameConfig(path);
            if (File.Exists(path))
            {
                foreach (var line in File.ReadAllLines(path))
                {
                    var trimmed = line.Trim();
                    if (string.IsNullOrEmpty(trimmed) || trimmed.StartsWith("#"))
                        continue;

                    int eq = trimmed.IndexOf('=');
                    if (eq <= 0) continue;

                    string key = trimmed.Substring(0, eq).Trim();
                    string raw = trimmed.Substring(eq + 1).Trim();
                    string value = Unquote(raw);
                    cfg._values[key] = value;
                }
            }
            return cfg;
        }

        public void Save()
        {
            var sb = new StringBuilder();

            var keys = _values.Keys.OrderBy(k => k, StringComparer.OrdinalIgnoreCase).ToList();
            foreach (var key in keys)
            {
                string value = _values[key];
                if (value == null) continue;

                if (IsNumber(value) || IsBool(value))
                    sb.AppendLine($"{key} = {value}");
                else
                    sb.AppendLine($"{key} = \"{Escape(value)}\"");
            }

            Directory.CreateDirectory(Path.GetDirectoryName(_path) ?? ".");
            File.WriteAllText(_path, sb.ToString(), Encoding.UTF8);
        }

        public string this[string key]
        {
            get { return _values.TryGetValue(key, out var v) ? v : null; }
            set { _values[key] = value ?? string.Empty; }
        }

        public T Get<T>(string key, T defaultValue) where T : IConvertible
        {
            if (!_values.TryGetValue(key, out var raw) || raw == null)
                return defaultValue;
            try
            {
                return (T)Convert.ChangeType(raw, typeof(T), CultureInfo.InvariantCulture);
            }
            catch
            {
                return defaultValue;
            }
        }

        public void Set<T>(string key, T value) where T : IConvertible
        {
            _values[key] = value?.ToString() ?? string.Empty;
        }

        private static string Unquote(string raw)
        {
            if (raw.Length >= 2 && raw.StartsWith("\"") && raw.EndsWith("\""))
            {
                var inner = raw.Substring(1, raw.Length - 2);
                return inner.Replace("\\\"", "\"").Replace("\\\\", "\\");
            }
            return raw;
        }

        private static string Escape(string value)
        {
            return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private static bool IsNumber(string value)
        {
            if (string.IsNullOrEmpty(value)) return false;
            return double.TryParse(value, NumberStyles.Any, CultureInfo.InvariantCulture, out _);
        }

        private static bool IsBool(string value)
        {
            return value.Equals("true", StringComparison.OrdinalIgnoreCase) ||
                   value.Equals("false", StringComparison.OrdinalIgnoreCase);
        }

        public string GameDataRoot
        {
            get { return this["game_data_root"]; }
            set { this["game_data_root"] = value; }
        }

        public string Resolution
        {
            get { return this["resolution"] ?? "1080p"; }
            set { this["resolution"] = value; }
        }

        public int ResolutionScale
        {
            get { return Get("resolution_scale", 1); }
            set { Set("resolution_scale", Math.Max(1, Math.Min(8, value))); }
        }

        public int AnisotropicOverride
        {
            get { return Get("anisotropic_override", -1); }
            set { Set("anisotropic_override", Math.Max(-1, Math.Min(16, value))); }
        }

        public string SwapPostEffect
        {
            get { return this["swap_post_effect"] ?? "none"; }
            set { this["swap_post_effect"] = value; }
        }

        public bool VSync
        {
            get { return Get("vsync", true); }
            set { Set("vsync", value); }
        }

        public bool Fullscreen
        {
            get { return Get("fullscreen", true); }
            set { Set("fullscreen", value); }
        }

        public string RenderTargetPath
        {
            get { return this["render_target_path_d3d12"] ?? "rov"; }
            set { this["render_target_path_d3d12"] = value; }
        }

        public string InputBackend
        {
            get { return this["input_backend"] ?? "sdl"; }
            set { this["input_backend"] = value; }
        }

        public string GlyphFamily
        {
            get { return this["glyph_family"] ?? "auto"; }
            set { this["glyph_family"] = value; }
        }

        public string LogLevel
        {
            get { return this["log_level"] ?? "off"; }
            set { this["log_level"] = value; }
        }

        public string AspectRatio
        {
            get { return this["aspect_ratio"] ?? "native"; }
            set { this["aspect_ratio"] = value; }
        }

        public string Renderer
        {
            get { return this["renderer"] ?? DisplayOptions.RendererNative; }
            set { this["renderer"] = value; }
        }

        public uint UserLanguage
        {
            get { return Get("user_language", 1u); }
            set { Set("user_language", value); }
        }

        public string LauncherLanguage
        {
            get { return this["launcher_language"] ?? LauncherLocalizer.DefaultLanguage; }
            set { this["launcher_language"] = LauncherLocalizer.NormalizeLanguage(value); }
        }

        public string KeybindA { get { return this["keybind_a"] ?? "Space"; } set { this["keybind_a"] = value; } }
        public string KeybindB { get { return this["keybind_b"] ?? "F"; } set { this["keybind_b"] = value; } }
        public string KeybindX { get { return this["keybind_x"] ?? "LMB"; } set { this["keybind_x"] = value; } }
        public string KeybindY { get { return this["keybind_y"] ?? "E"; } set { this["keybind_y"] = value; } }
        public string KeybindLeftShoulder { get { return this["keybind_left_shoulder"] ?? "Q"; } set { this["keybind_left_shoulder"] = value; } }
        public string KeybindRightShoulder { get { return this["keybind_right_shoulder"] ?? "RMB"; } set { this["keybind_right_shoulder"] = value; } }
        public string KeybindLeftTrigger { get { return this["keybind_left_trigger"] ?? "Shift"; } set { this["keybind_left_trigger"] = value; } }
        public string KeybindRightTrigger { get { return this["keybind_right_trigger"] ?? "Ctrl"; } set { this["keybind_right_trigger"] = value; } }
        public string KeybindLStickUp { get { return this["keybind_lstick_up"] ?? "W"; } set { this["keybind_lstick_up"] = value; } }
        public string KeybindLStickDown { get { return this["keybind_lstick_down"] ?? "S"; } set { this["keybind_lstick_down"] = value; } }
        public string KeybindLStickLeft { get { return this["keybind_lstick_left"] ?? "A"; } set { this["keybind_lstick_left"] = value; } }
        public string KeybindLStickRight { get { return this["keybind_lstick_right"] ?? "D"; } set { this["keybind_lstick_right"] = value; } }
        public string KeybindLStickPress { get { return this["keybind_lstick_press"] ?? "X"; } set { this["keybind_lstick_press"] = value; } }
        public string KeybindRStickUp { get { return this["keybind_rstick_up"] ?? "Up"; } set { this["keybind_rstick_up"] = value; } }
        public string KeybindRStickDown { get { return this["keybind_rstick_down"] ?? "Down"; } set { this["keybind_rstick_down"] = value; } }
        public string KeybindRStickLeft { get { return this["keybind_rstick_left"] ?? "Left"; } set { this["keybind_rstick_left"] = value; } }
        public string KeybindRStickRight { get { return this["keybind_rstick_right"] ?? "Right"; } set { this["keybind_rstick_right"] = value; } }
        public string KeybindRStickPress { get { return this["keybind_rstick_press"] ?? "R"; } set { this["keybind_rstick_press"] = value; } }
        public string KeybindDpadUp { get { return this["keybind_dpad_up"] ?? "Shift+Up"; } set { this["keybind_dpad_up"] = value; } }
        public string KeybindDpadDown { get { return this["keybind_dpad_down"] ?? "Shift+Down"; } set { this["keybind_dpad_down"] = value; } }
        public string KeybindDpadLeft { get { return this["keybind_dpad_left"] ?? "Shift+Left"; } set { this["keybind_dpad_left"] = value; } }
        public string KeybindDpadRight { get { return this["keybind_dpad_right"] ?? "Shift+Right"; } set { this["keybind_dpad_right"] = value; } }
        public string KeybindBack { get { return this["keybind_back"] ?? "Tab"; } set { this["keybind_back"] = value; } }
        public string KeybindStart { get { return this["keybind_start"] ?? "Escape"; } set { this["keybind_start"] = value; } }

        public bool Remove(string key)
        {
            if (string.IsNullOrEmpty(key))
                return false;
            return _values.Remove(key);
        }

        public bool ContainsKey(string key)
        {
            return !string.IsNullOrEmpty(key) && _values.ContainsKey(key);
        }

        public bool MigrateLegacySettings()
        {
            bool changed = false;

            if (Remove("aspect_ratio"))
                changed = true;
            if (Remove("resolution"))
                changed = true;

            int rawScale = Get("resolution_scale", DisplayOptions.MinScale);
            int clampedScale = DisplayOptions.ClampScale(rawScale);
            if (!ContainsKey("resolution_scale") || rawScale != clampedScale ||
                this["resolution_scale"] != clampedScale.ToString(CultureInfo.InvariantCulture))
            {
                ResolutionScale = clampedScale;
                changed = true;
            }

            string renderer = DisplayOptions.NormalizeRenderer(this["renderer"]);
            if (!ContainsKey("renderer") || this["renderer"] != renderer)
            {
                Renderer = renderer;
                changed = true;
            }

            return changed;
        }
    }
}
