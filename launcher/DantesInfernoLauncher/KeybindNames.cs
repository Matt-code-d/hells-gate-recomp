using System.Collections.Generic;
using System.Windows.Input;

namespace DantesInferno
{
    public static class KeybindNames
    {
        private static readonly Dictionary<Key, string> KeyMap = new Dictionary<Key, string>
        {
            { Key.Back, "Backspace" },
            { Key.Tab, "Tab" },
            { Key.Return, "Return" },
            { Key.Escape, "Escape" },
            { Key.Space, "Space" },
            { Key.Prior, "PageUp" },
            { Key.Next, "PageDown" },
            { Key.End, "End" },
            { Key.Home, "Home" },
            { Key.Left, "Left" },
            { Key.Up, "Up" },
            { Key.Right, "Right" },
            { Key.Down, "Down" },
            { Key.Insert, "Insert" },
            { Key.Delete, "Delete" },
            { Key.Capital, "CapsLock" },
            { Key.NumLock, "NumLock" },
            { Key.Scroll, "ScrollLock" },
            { Key.Snapshot, "PrintScreen" },
            { Key.Pause, "Pause" },
            { Key.Oem1, "Semicolon" },
            { Key.Oem2, "Slash" },
            { Key.Oem3, "Backtick" },
            { Key.Oem4, "LBracket" },
            { Key.Oem5, "Backslash" },
            { Key.Oem6, "RBracket" },
            { Key.Oem7, "Quote" },
            { Key.Oem102, "Oem102" },
            { Key.OemMinus, "Minus" },
            { Key.OemPlus, "Plus" },
            { Key.OemComma, "Comma" },
            { Key.OemPeriod, "Period" },
            { Key.Add, "NumpadPlus" },
            { Key.Subtract, "NumpadMinus" },
            { Key.Multiply, "NumpadStar" },
            { Key.Divide, "NumpadSlash" },
            { Key.Decimal, "NumpadDecimal" },
            { Key.LeftShift, "Shift" },
            { Key.RightShift, "Shift" },
            { Key.LeftCtrl, "Ctrl" },
            { Key.RightCtrl, "Ctrl" },
            { Key.LeftAlt, "Alt" },
            { Key.RightAlt, "Alt" },
            { Key.System, "Alt" },
        };

        static KeybindNames()
        {
            for (int i = 0; i <= 9; i++)
            {
                KeyMap[(Key)((int)Key.D0 + i)] = ((char)('0' + i)).ToString();
                KeyMap[(Key)((int)Key.NumPad0 + i)] = "Numpad" + i;
            }
            for (int i = 0; i < 26; i++)
                KeyMap[(Key)((int)Key.A + i)] = ((char)('A' + i)).ToString();
            for (int i = 0; i < 24; i++)
                KeyMap[(Key)((int)Key.F1 + i)] = "F" + (i + 1);
        }

        public static bool IsModifier(Key key)
        {
            return key == Key.LeftShift || key == Key.RightShift ||
                   key == Key.LeftCtrl || key == Key.RightCtrl ||
                   key == Key.LeftAlt || key == Key.RightAlt ||
                   key == Key.System || key == Key.LWin || key == Key.RWin;
        }

        public static string FromWpfKey(Key key)
        {
            string name;
            return KeyMap.TryGetValue(key, out name) ? name : null;
        }

        public static string FromWpfMouseButton(MouseButton button)
        {
            switch (button)
            {
                case MouseButton.Left: return "LMB";
                case MouseButton.Right: return "RMB";
                case MouseButton.Middle: return "MMB";
                case MouseButton.XButton1: return "Mouse4";
                case MouseButton.XButton2: return "Mouse5";
                default: return null;
            }
        }

        public static string ModifierPrefix()
        {
            var mods = new List<string>();
            if (Keyboard.Modifiers.HasFlag(ModifierKeys.Shift)) mods.Add("Shift");
            if (Keyboard.Modifiers.HasFlag(ModifierKeys.Control)) mods.Add("Ctrl");
            if (Keyboard.Modifiers.HasFlag(ModifierKeys.Alt)) mods.Add("Alt");
            return mods.Count > 0 ? string.Join("+", mods) + "+" : string.Empty;
        }
    }
}
