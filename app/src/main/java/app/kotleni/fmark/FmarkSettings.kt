package app.kotleni.fmark

enum class PresentMode(val label: String) {
    UNCAPPED("Uncapped"),
    VSYNC("Vsync"),
}

enum class Antialiasing(val label: String, val samples: Int) {
    OFF("Off", 0),
    MSAA_2X("2× MSAA", 2),
    MSAA_4X("4× MSAA", 4),
    MSAA_8X("8× MSAA", 8),
}

enum class ResolutionOption(val label: String, val width: Int = 0, val height: Int = 0) {
    // Ultra-Wide & High-End Desktop
    R8K("7680 × 4320", 7680, 4320),
    R5K("5120 × 2880", 5120, 2880),
    UWQHD("3440 × 1440", 3440, 1440),
    R4K("3840 × 2160", 3840, 2160),

    // Workstation & Desktop Standards
    WQXGA("2560 × 1600", 2560, 1600),
    QHD("2560 × 1440", 2560, 1440),
    WUXGA("1920 × 1200", 1920, 1200),
    FHD("1920 × 1080", 1920, 1080),
    WXGA_PLUS("1440 × 900", 1440, 900),
    HDPLUS("1366 × 768", 1366, 768),
    HD("1280 × 720", 1280, 720),

    // Modern Mobile / Foldable / Tablet Native Enums (Landscape)
    FHD_PLUS_MOBILE("2400 × 1080", 2400, 1080), // Modern Android 20:9
    FHD_PLUS_TALL("2340 × 1080", 2340, 1080),   // Samsung Galaxy / Pixel Standard
    QHD_PLUS_MOBILE("3080 × 1440", 3080, 1440), // Flagship Mobile (S24 Ultra / Pixel Pro)
    RETINA_IPAD_PRO("2752 × 2064", 2752, 2064), // iPad Pro 11"
    RETINA_IPAD_AIR("2360 × 1640", 2360, 1640), // iPad Air / Mini 6
    RETINA_IPHONE_MAX("2796 × 1290", 2796, 1290), // iPhone Pro Max series

    // Legacy Displays & Low-End
    SXGA("1280 × 1024", 1280, 1024),
    XGA("1024 × 768", 1024, 768),
    SVGA("800 × 600", 800, 600),
    VGA("640 × 480", 640, 480),
    HVGA("480 × 320", 480, 320),
    QVGA("320 × 240", 320, 240)
}

data class FmarkSettings(
    var resolution: ResolutionOption = ResolutionOption.FHD,
    var presentMode: PresentMode = PresentMode.UNCAPPED,
    var antialiasing: Antialiasing = Antialiasing.OFF,
    var furLength: Float = DEFAULT_FUR_LENGTH,
    var furPopulation: Int = DEFAULT_FUR_POPULATION,
    var showOverlays: Boolean = true,
) {
    companion object {
        const val MIN_FUR_POPULATION = 25_000
        const val MAX_FUR_POPULATION = 1_000_000
        const val FUR_POPULATION_STEP = 25_000
        const val DEFAULT_FUR_LENGTH = 0.25f
        const val DEFAULT_FUR_POPULATION = 25_000
    }
}