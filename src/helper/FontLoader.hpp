#pragma once

class FontLoader {
    public:
        // Nạp font một lần duy nhất (idempotent)
        static void loadCustomFontOnce();
};
