using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.InteropServices;

namespace DantesInferno
{
    public class DisplayAspect
    {
        public string Name { get; set; }
        public double Value { get; set; }
        public bool IsNative { get; set; }
        public int DetectedWidth { get; set; }
        public int DetectedHeight { get; set; }
    }

    public class DisplayModeOption
    {
        public string Label { get; set; }
        public int Scale { get; set; }
        public int Width { get; set; }
        public int Height { get; set; }
    }

    public static class DisplayOptions
    {
        public const int GuestBaseHeight = 720;
        public const int MinScale = 1;
        public const int MaxScale = 3;
        public const double NativeAspect = 1.7778;

        public const string RendererNative = "native";
        public const string RendererReXGlue = "rexglue";

        [DllImport("user32.dll")]
        private static extern int GetSystemMetrics(int nIndex);

        public static DisplayAspect DetectPrimaryAspect()
        {
            int width = 0;
            int height = 0;
            try
            {
                width = GetSystemMetrics(0);
                height = GetSystemMetrics(1);
            }
            catch
            {
                width = 0;
                height = 0;
            }

            if (width <= 0 || height <= 0)
            {
                width = 1920;
                height = 1080;
            }

            var aspect = ClassifyAspect((double)width / height);
            aspect.DetectedWidth = width;
            aspect.DetectedHeight = height;
            return aspect;
        }

        public static DisplayAspect ClassifyAspect(double rawAspect)
        {
            if (!(rawAspect > 0.1) || double.IsNaN(rawAspect) || double.IsInfinity(rawAspect))
                rawAspect = NativeAspect;

            var known = new List<DisplayAspect>
            {
                new DisplayAspect { Name = "4:3", Value = 1.3333, IsNative = false },
                new DisplayAspect { Name = "16:10", Value = 1.6, IsNative = false },
                new DisplayAspect { Name = "16:9", Value = NativeAspect, IsNative = true },
                new DisplayAspect { Name = "21:9", Value = 2.3889, IsNative = false },
                new DisplayAspect { Name = "32:9", Value = 3.5556, IsNative = false },
            };

            const double tolerance = 0.06;
            foreach (var candidate in known)
            {
                if (Math.Abs(rawAspect - candidate.Value) <= tolerance)
                    return new DisplayAspect
                    {
                        Name = candidate.Name,
                        Value = candidate.Value,
                        IsNative = candidate.IsNative,
                    };
            }

            double clamped = rawAspect;
            if (clamped < 1.3333) clamped = 1.3333;
            if (clamped > 3.5556) clamped = 3.5556;
            clamped = Math.Round(clamped, 4);

            return new DisplayAspect
            {
                Name = "Custom (" + clamped.ToString("0.####", CultureInfo.InvariantCulture) + ":1)",
                Value = clamped,
                IsNative = Math.Abs(clamped - NativeAspect) <= 0.0002,
            };
        }

        public static List<DisplayModeOption> BuildResolutionOptions(DisplayAspect aspect)
        {
            var options = new List<DisplayModeOption>();
            if (aspect == null)
                aspect = ClassifyAspect(NativeAspect);

            for (int scale = MinScale; scale <= MaxScale; scale++)
            {
                int height = GuestBaseHeight * scale;
                int width = RoundToEven(aspect.Value * height);
                string suffix;
                if (scale == 1)
                    suffix = "1x (original)";
                else if (scale == 2)
                    suffix = "2x";
                else
                    suffix = scale + "x (4K class)";

                options.Add(new DisplayModeOption
                {
                    Label = string.Format(CultureInfo.InvariantCulture, "{0}x{1} - {2}", width, height, suffix),
                    Scale = scale,
                    Width = width,
                    Height = height,
                });
            }
            return options;
        }

        public static int RoundToEven(double value)
        {
            int rounded = (int)Math.Round(value, MidpointRounding.AwayFromZero);
            if ((rounded & 1) != 0)
                rounded += 1;
            return rounded;
        }

        public static int ClampScale(int scale)
        {
            if (scale < MinScale) return MinScale;
            if (scale > MaxScale) return MaxScale;
            return scale;
        }

        public static string NormalizeRenderer(string renderer)
        {
            if (!string.IsNullOrEmpty(renderer) &&
                renderer.Equals(RendererNative, StringComparison.OrdinalIgnoreCase))
                return RendererNative;
            return RendererReXGlue;
        }

        public static string FormatAspectValue(double value)
        {
            return value.ToString("0.####", CultureInfo.InvariantCulture);
        }

        public static string BuildLaunchArguments(GameConfig config, string gameDataRoot, DisplayAspect aspect)
        {
            if (config == null)
                throw new ArgumentNullException("config");
            if (aspect == null)
                aspect = ClassifyAspect(NativeAspect);

            bool loggingEnabled = !config.LogLevel.Equals("off", StringComparison.OrdinalIgnoreCase);
            string renderer = NormalizeRenderer(config.Renderer);

            var args = new List<string>();
            args.Add(string.Format("--game_data_root=\"{0}\"", gameDataRoot));

            args.Add("--render_target_path_d3d12=rov");

            int scale = ClampScale(config.ResolutionScale);
            if (scale > 1)
                args.Add(string.Format(CultureInfo.InvariantCulture, "--resolution_scale={0}", scale));

            if (!string.IsNullOrEmpty(config.SwapPostEffect) && config.SwapPostEffect != "none")
                args.Add(string.Format("--swap_post_effect={0}", config.SwapPostEffect));

            if (config.AnisotropicOverride >= 0)
                args.Add(string.Format(CultureInfo.InvariantCulture, "--anisotropic_override={0}", config.AnisotropicOverride));

            args.Add("--vsync=true");
            args.Add("--d3d12_host_vsync=true");
            args.Add("--video_mode_refresh_rate=60");

            args.Add("--input_backend=" + config.InputBackend);

            if (!aspect.IsNative)
                args.Add(string.Format(CultureInfo.InvariantCulture,
                    "--ultrawide_target_aspect={0}", FormatAspectValue(aspect.Value)));

            if (renderer == RendererNative)
            {
                args.Add("--use_native_presenter=true");
                args.Add("--use_gpu_interop=true");
            }

            args.Add(loggingEnabled ? "--log_level=info" : "--log_level=off");

            if (config.GlyphFamily.Equals("playstation", StringComparison.OrdinalIgnoreCase))
                args.Add("--glyph_family=playstation");

            return string.Join(" ", args);
        }
    }
}
