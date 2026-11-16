#include <SFSE/SFSE.h>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string_view>
#include <utility>

namespace {
constexpr REL::Version supportedRuntime{ 1, 16, 244, 0 };

using DetailsInputHandler = bool (*)(void*, std::uint8_t*);
DetailsInputHandler originalDetailsInput = nullptr;

using SetWidgetPosition = void (*)(void*, float, float);
float fullscreenArrowCenterY = 0.0f;
float fullscreenArrowLeftX = 0.0f;
float fullscreenArrowRightX = 0.0f;

void SetFullscreenArrowPosition(void* arrow, float x, float y)
{
    const auto* vtable = *reinterpret_cast<std::uintptr_t* const*>(arrow);
    reinterpret_cast<SetWidgetPosition>(vtable[7])(arrow, x, y);
}

void PositionFullscreenLeftArrow(void* arrow, float, float)
{
    SetFullscreenArrowPosition(arrow, fullscreenArrowLeftX, fullscreenArrowCenterY);
}

void PositionFullscreenRightArrow(void* arrow, float, float)
{
    SetFullscreenArrowPosition(arrow, fullscreenArrowRightX, fullscreenArrowCenterY);
}

bool HandleHorizontalDetailsInput(void* page, std::uint8_t* input)
{
    const auto* details = static_cast<const std::byte*>(page);
    std::uint32_t focus = 0;
    std::int32_t selection = 0;
    std::memcpy(&focus, details + 0xA0, sizeof(focus));
    std::memcpy(&selection, details + 0xF8, sizeof(selection));
    if (focus == 0) {
        constexpr auto left = 0x40;
        constexpr auto right = 0x54;
        constexpr auto up = 0x68;
        constexpr auto down = 0x7C;
        // At the first action, Up can still return focus to the gallery.
        // Left at that boundary stays on the action row.
        input[up] = selection > 0 ? input[left] : (input[left] || input[right] ? 0 : input[up]);
        input[down] = input[right];
        input[left] = 0;
        input[right] = 0;
    }
    return originalDetailsInput(page, input);
}

void UpdateMarketplaceBytes(std::uintptr_t base, std::uintptr_t rva, const void* stock,
    const void* value, std::size_t size, bool executable)
{
    auto* address = reinterpret_cast<void*>(base + rva);
    if (std::memcmp(address, stock, size) != 0) {
        REX::WARN("Ultrawide Creations Menu: Marketplace polish skipped at RVA {:#x}: unexpected stock value", rva);
        return;
    }
    DWORD oldProtection = 0;
    if (!VirtualProtect(address, size, executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE,
            &oldProtection)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace polish skipped at RVA {:#x}: protection change failed", rva);
        return;
    }
    std::memcpy(address, value, size);
    if (executable && !FlushInstructionCache(GetCurrentProcess(), address, size)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace polish at RVA {:#x}: cache flush failed", rva);
    }
    DWORD unused = 0;
    if (!VirtualProtect(address, size, oldProtection, &unused)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace polish at RVA {:#x}: protection restore failed", rva);
    }
}

template <class T>
void UpdateMarketplaceValue(std::uintptr_t base, std::uintptr_t rva, T stock, T value, bool executable)
{
    UpdateMarketplaceBytes(base, rva, &stock, &value, sizeof(T), executable);
}

void InstallMarketplaceDetailBounds(std::uintptr_t base, float aspect)
{
    const float logicalHeight = 3840.0f / aspect;
    // The bottom command bar occupies the last part of the viewport.
    const float shift = std::clamp(1973.0f + 96.0f - logicalHeight, 0.0f, 600.0f);
    if (shift == 0.0f) {
        return;
    }

    struct RectanglePatch
    {
        std::uintptr_t rva;
        std::array<float, 4> stock;
        std::array<float, 4> replacement;
    };
    constexpr std::uintptr_t actionOrientationRva = 0x2C3921D;
    constexpr std::uintptr_t actionWidgetWidthRva = 0x4F05FD8;
    constexpr std::uint32_t stockOrientation = 2;
    constexpr float stockActionWidgetWidth = 1020.0f;
    if (std::memcmp(reinterpret_cast<const void*>(base + actionOrientationRva),
            &stockOrientation, sizeof(stockOrientation)) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(base + actionWidgetWidthRva),
            &stockActionWidgetWidth, sizeof(stockActionWidgetWidth)) != 0) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details skipped: unexpected stock action geometry");
        return;
    }

    constexpr float actionRowHeight = 116.0f;
    constexpr float galleryWidth = 2048.0f;
    constexpr float thumbnailInset = galleryWidth * 0.05f;
    constexpr float thumbnailLift = 14.0f;
    constexpr float previewTrim = 34.0f;
    constexpr float fullscreenVerticalInset = 168.0f;
    constexpr float actionWidgetWidth = 460.0f;
    constexpr float actionGap = 28.0f;
    constexpr float actionSlotWidth = actionWidgetWidth + actionGap;
    constexpr float actionListWidth = 5.0f * actionSlotWidth;
    constexpr float visibleActionWidth = 3.0f * actionSlotWidth + actionWidgetWidth;
    static_assert(visibleActionWidth <= galleryWidth);
    constexpr float actionLeft = 100.0f + (galleryWidth - visibleActionWidth) / 2.0f;
    const float galleryShift = (std::max)(0.0f, shift - (400.0f - actionRowHeight));
    const float previewHeight = 1152.0f - galleryShift - previewTrim;
    const float previewWidth = previewHeight * 16.0f / 9.0f;
    const float previewLeft = (galleryWidth - previewWidth) / 2.0f - thumbnailInset;
    const float fullscreenHeight = (std::min)(1823.0f, logicalHeight - 2.0f * fullscreenVerticalInset);
    const float fullscreenWidth = fullscreenHeight * 16.0f / 9.0f;
    const std::array rectangles{
        RectanglePatch{ 0x4F06380, { 0, 0, 2048, 1152 }, { previewLeft, 0, previewLeft + previewWidth, previewHeight } },
        RectanglePatch{ 0x4F06420, { 2170, 180, 3740, 1758 }, { 2170, 180, 3740, 1758 - shift } },
        RectanglePatch{ 0x4F06450, { 0, 0, 3240, 1823 }, { 0, 0, fullscreenWidth, fullscreenHeight } },
        RectanglePatch{ 0x4F06470, { 2170, 1780, 3740, 1845 }, { 2170, 1780 - shift, 3740, 1845 - shift } },
        RectanglePatch{ 0x4F064C0, { 2170, 1850, 3740, 1921 }, { 2170, 1850 - shift, 3740, 1921 - shift } },
        RectanglePatch{ 0x4F064D0, { 100, 1573, 1120, 1973 },
            { actionLeft, 1573 - galleryShift, actionLeft + actionListWidth, 1573 - galleryShift + actionRowHeight } }
    };
    constexpr float galleryBottom = 1544.0f;
    constexpr std::uintptr_t galleryLeftRva = 0x2C68B44;
    constexpr std::uintptr_t galleryRightRva = 0x2C68B58;
    constexpr std::uintptr_t galleryBottomRva = 0x2C68B62;
    constexpr std::uintptr_t scissorLeftRva = 0x2C68B81;
    constexpr std::uintptr_t scissorRightLoadRva = 0x2C68B6E;
    constexpr std::array<unsigned char, 8> stockRightLoad{
        0xC5, 0xFA, 0x10, 0x0D, 0x96, 0xC3, 0x29, 0x02
    };
    constexpr std::array<unsigned char, 8> insetRightLoad{
        0xC5, 0xFA, 0x10, 0x8B, 0xE8, 0x00, 0x00, 0x00
    };
    constexpr std::uintptr_t scrollbarRva = 0x2C68D2B;
    constexpr std::array<unsigned char, 18> stockScrollbar{
        0xC5, 0xFA, 0x10, 0x4B, 0x2C, 0xC5, 0xF2, 0x5C, 0x43, 0x24,
        0xC5, 0xFA, 0x58, 0x35, 0x7B, 0xBC, 0x29, 0x02
    };
    // Position the track using the saved compact gallery bottom, while
    // retaining the constructor's vertical bounds for thumbnail clipping.
    constexpr std::array<unsigned char, 18> compactScrollbar{
        0xC5, 0xFA, 0x10, 0x8B, 0xEC, 0x00, 0x00, 0x00,
        0xC5, 0xF2, 0x5C, 0x43, 0x24, 0xC5, 0xF8, 0x28, 0xF0, 0x90
    };
    auto* gallery = reinterpret_cast<void*>(base + galleryLeftRva);
    auto* scrollbar = reinterpret_cast<void*>(base + scrollbarRva);
    constexpr auto galleryCodeLength = scrollbarRva - galleryLeftRva + compactScrollbar.size();
    constexpr float stockLeft = 100.0f;
    constexpr float stockRight = stockLeft + galleryWidth;
    if (std::memcmp(gallery, &stockLeft, sizeof(stockLeft)) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(base + galleryRightRva), &stockRight, sizeof(stockRight)) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(base + galleryBottomRva), &galleryBottom, sizeof(galleryBottom)) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(base + scissorLeftRva), &stockLeft, sizeof(stockLeft)) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(base + scissorRightLoadRva), stockRightLoad.data(), stockRightLoad.size()) != 0) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details skipped: unexpected gallery bounds");
        return;
    }
    if (std::memcmp(scrollbar, stockScrollbar.data(), stockScrollbar.size()) != 0) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details skipped: unexpected scrollbar positioning");
        return;
    }
    for (const auto& rectangle : rectangles) {
        if (std::memcmp(reinterpret_cast<const void*>(base + rectangle.rva),
                rectangle.stock.data(), sizeof(rectangle.stock)) != 0) {
            REX::WARN("Ultrawide Creations Menu: Marketplace details skipped: unexpected rectangle at RVA {:#x}", rectangle.rva);
            return;
        }
    }

    auto* firstRectangle = reinterpret_cast<void*>(base + rectangles.front().rva);
    const auto length = rectangles.back().rva - rectangles.front().rva + sizeof(rectangles.back().stock);
    DWORD galleryProtection = 0;
    DWORD rectangleProtection = 0;
    if (!VirtualProtect(gallery, galleryCodeLength, PAGE_EXECUTE_READWRITE, &galleryProtection)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details skipped: gallery protection change failed ({})", GetLastError());
        return;
    }
    DWORD unused = 0;
    if (!VirtualProtect(firstRectangle, length, PAGE_READWRITE, &rectangleProtection)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details skipped: rectangle protection change failed ({})", GetLastError());
        if (!VirtualProtect(gallery, galleryCodeLength, galleryProtection, &unused)) {
            REX::WARN("Ultrawide Creations Menu: Marketplace details: gallery protection restore failed ({})", GetLastError());
        }
        return;
    }
    const float newGalleryLeft = stockLeft + thumbnailInset;
    const float newGalleryRight = stockRight - thumbnailInset;
    const float newGalleryBottom = galleryBottom - galleryShift - thumbnailLift;
    std::memcpy(gallery, &newGalleryLeft, sizeof(newGalleryLeft));
    std::memcpy(reinterpret_cast<void*>(base + galleryRightRva), &newGalleryRight, sizeof(newGalleryRight));
    std::memcpy(reinterpret_cast<void*>(base + galleryBottomRva), &newGalleryBottom, sizeof(newGalleryBottom));
    std::memcpy(reinterpret_cast<void*>(base + scissorLeftRva), &newGalleryLeft, sizeof(newGalleryLeft));
    std::memcpy(reinterpret_cast<void*>(base + scissorRightLoadRva), insetRightLoad.data(), insetRightLoad.size());
    std::memcpy(scrollbar, compactScrollbar.data(), compactScrollbar.size());
    if (!FlushInstructionCache(GetCurrentProcess(), gallery, galleryCodeLength)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details: gallery instruction cache flush failed ({})", GetLastError());
    }
    for (const auto& rectangle : rectangles) {
        std::memcpy(reinterpret_cast<void*>(base + rectangle.rva),
            rectangle.replacement.data(), sizeof(rectangle.replacement));
    }
    if (!VirtualProtect(firstRectangle, length, rectangleProtection, &unused)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details: rectangle protection restore failed ({})", GetLastError());
    }
    if (!VirtualProtect(gallery, galleryCodeLength, galleryProtection, &unused)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace details: gallery protection restore failed ({})", GetLastError());
    }
    REX::INFO("Ultrawide Creations Menu: Marketplace details: logical height {:.1f}, detail bounds reduced by {:.1f}", logicalHeight, shift);
    // The fullscreen gallery uses the current ultrawide width for horizontal
    // centering, but retains a 2160-high coordinate space vertically. Keep
    // the stock vertical anchor so the shorter image starts below the header.
    UpdateMarketplaceValue(base, 0x4F04EE8, 1620.0f, fullscreenWidth / 2.0f, false);
    UpdateMarketplaceValue(base, 0x4F04F6C, 3192.0f, fullscreenWidth - 48.0f, false);
    fullscreenArrowCenterY = fullscreenVerticalInset + fullscreenHeight / 2.0f;
    fullscreenArrowLeftX = (3840.0f - fullscreenWidth) / 2.0f - 64.0f;
    fullscreenArrowRightX = (3840.0f + fullscreenWidth) / 2.0f + 64.0f;
    constexpr std::array<unsigned char, 14> leftArrowPosition{
        0x48, 0x8B, 0x01, 0xC5, 0xCA, 0x58, 0x0D, 0x7D, 0xAD, 0x29, 0x02, 0xFF, 0x50, 0x38
    };
    constexpr std::array<unsigned char, 14> rightArrowPosition{
        0x48, 0x8B, 0x01, 0xC5, 0xCA, 0x58, 0x0D, 0xD2, 0xB0, 0x29, 0x02, 0xFF, 0x50, 0x38
    };
    std::array<unsigned char, 14> arrowHook{};
    arrowHook.fill(0x90);
    constexpr std::uintptr_t leftArrowPositionRva = 0x2C69E6C;
    constexpr std::uintptr_t rightArrowPositionRva = 0x2C69E8F;
    auto& arrowTrampoline = REL::GetTrampoline();
    if (arrowTrampoline.free_size() >= 2 * sizeof(std::uintptr_t)) {
        UpdateMarketplaceValue(base, leftArrowPositionRva, leftArrowPosition, arrowHook, true);
        UpdateMarketplaceValue(base, rightArrowPositionRva, rightArrowPosition, arrowHook, true);
        if (std::memcmp(reinterpret_cast<const void*>(base + leftArrowPositionRva), arrowHook.data(), arrowHook.size()) == 0 &&
            std::memcmp(reinterpret_cast<const void*>(base + rightArrowPositionRva), arrowHook.data(), arrowHook.size()) == 0) {
            arrowTrampoline.write_call<5>(base + leftArrowPositionRva, &PositionFullscreenLeftArrow);
            arrowTrampoline.write_call<5>(base + rightArrowPositionRva, &PositionFullscreenRightArrow);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(base + leftArrowPositionRva), arrowHook.size());
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(base + rightArrowPositionRva), arrowHook.size());
        }
    } else {
        REX::WARN("Ultrawide Creations Menu: fullscreen arrow alignment unavailable");
    }
    // This rectangle is used only by the detail-action constructors. Match
    // their initial child widths to the horizontal list's smaller slots.
    UpdateMarketplaceValue(base, actionWidgetWidthRva, stockActionWidgetWidth, actionWidgetWidth, false);
    UpdateMarketplaceValue(base, actionOrientationRva, stockOrientation, std::uint32_t{1}, true);
    constexpr std::uintptr_t detailsInputSlot = 0x4EFCBB8;
    originalDetailsInput = reinterpret_cast<DetailsInputHandler>(base + 0x2C3F6B0);
    UpdateMarketplaceValue(base, detailsInputSlot, originalDetailsInput, &HandleHorizontalDetailsInput, false);
    // The action labels use explicit font sizes in the detail constructor.
    // Keep the compact labels so the download size and ownership text can share a slot.
    constexpr std::array<std::uintptr_t, 4> actionLabelSizes{
        0x2C38F72, 0x2C38F90, 0x2C38FAE, 0x2C39426
    };
    for (const auto rva : actionLabelSizes) {
        UpdateMarketplaceValue(base, rva, std::uint8_t{48}, std::uint8_t{28}, true);
    }
    UpdateMarketplaceValue(base, 0x2C38FC6, std::uint8_t{49}, std::uint8_t{26}, true);
    UpdateMarketplaceValue(base, 0x2C38FCC, std::uint8_t{40}, std::uint8_t{24}, true);
    // The sale badge sits before the credit icon and prices in the Purchase
    // button's inner layout. Its stock 420px reservation pushes them out of
    // the compact button, even when the badge itself is hidden.
    constexpr float saleBadgeWidth = 112.0f;
    UpdateMarketplaceValue(base, 0x4F04DE4, 420.0f, saleBadgeWidth, false);
    UpdateMarketplaceValue(base, 0x4F05F58, 420.0f, saleBadgeWidth, false);
    UpdateMarketplaceValue(base, 0x4F05F98, 420.0f, saleBadgeWidth, false);
    UpdateMarketplaceValue(base, 0x2C435E7, 40.0f, 26.0f, true);
    UpdateMarketplaceValue(base, 0x2C66BFD, 40.0f, 34.0f, true);
    UpdateMarketplaceValue(base, 0x4F0421C, 0.24725276f, 0.185f, false);
    // The detail panel creates each tag label separately from its text body.
    UpdateMarketplaceValue(base, 0x2C676E6, 45.0f, 35.0f, true);
    UpdateMarketplaceValue(base, 0x2C66854, std::uint32_t{47}, std::uint32_t{35}, true);

    // Use the same paragraph gap when building, measuring, and positioning
    // the rich text so the description and its scroll range stay in sync.
    constexpr std::int32_t stockParagraphGap = 0x4F04BE0;  // 40 logical pixels
    constexpr std::int32_t compactParagraphGap = 0x4F04B1C;  // 20 logical pixels
    constexpr std::array<std::int32_t, 3> paragraphGapOperands{
        0x2C740BF, 0x2C748B0, 0x2C749A0
    };
    for (const auto operand : paragraphGapOperands) {
        const auto nextInstruction = operand + 4;
        UpdateMarketplaceValue(base, operand, stockParagraphGap - nextInstruction,
            compactParagraphGap - nextInstruction, true);
    }

    // Both normal gallery arrow positions load this same vertical midpoint.
    constexpr std::uintptr_t arrowCenterRva = 0x4F04E1C;
    constexpr float stockArrowCenter = 576.0f;
    auto* arrowCenter = reinterpret_cast<void*>(base + arrowCenterRva);
    if (std::memcmp(arrowCenter, &stockArrowCenter, sizeof(stockArrowCenter)) != 0) {
        REX::WARN("Ultrawide Creations Menu: Marketplace arrow alignment skipped: unexpected stock midpoint");
        return;
    }
    DWORD arrowProtection = 0;
    if (!VirtualProtect(arrowCenter, sizeof(stockArrowCenter), PAGE_READWRITE, &arrowProtection)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace arrow alignment skipped: protection change failed ({})", GetLastError());
        return;
    }
    const float previewCenter = previewHeight / 2.0f;
    std::memcpy(arrowCenter, &previewCenter, sizeof(previewCenter));
    if (!VirtualProtect(arrowCenter, sizeof(stockArrowCenter), arrowProtection, &unused)) {
        REX::WARN("Ultrawide Creations Menu: Marketplace arrows: protection restore failed ({})", GetLastError());
    }
}

void InstallMarketplaceResultsHeading(std::uintptr_t base)
{
    // SearchResultsPage adds 450 logical pixels to both heading bounds
    // when the results fit on one row. In the wide layout that puts the
    // heading behind the cards. Use the same 20px inset as the two-row case.
    constexpr std::uintptr_t singleRowHeadingRva = 0x2C46B38;
    constexpr std::array<unsigned char, 16> stock{
        0xC5, 0x3A, 0x58, 0x05, 0xB8, 0xE2, 0x2B, 0x02,
        0xC5, 0x32, 0x58, 0x0D, 0xB0, 0xE2, 0x2B, 0x02
    };
    constexpr std::array<unsigned char, 16> compact{
        0xC5, 0x3A, 0x58, 0x05, 0xDC, 0xDF, 0x2B, 0x02,
        0xC5, 0x32, 0x58, 0x0D, 0xD4, 0xDF, 0x2B, 0x02
    };
    UpdateMarketplaceValue(base, singleRowHeadingRva, stock, compact, true);
}

void RedirectMarketplaceConstant(std::uintptr_t base, std::int32_t operand,
    std::int32_t stock, std::int32_t replacement)
{
    const auto nextInstruction = operand + 4;
    UpdateMarketplaceValue(base, operand, stock - nextInstruction,
        replacement - nextInstruction, true);
}

void InstallCompactMarketplaceResultsGrid(std::uintptr_t base)
{
    using namespace std::literals;
    struct GridPatch
    {
        std::uintptr_t rva;
        std::string_view stock;
        std::string_view replacement;
    };
    // The native grid inlines its five-column arithmetic. Use eight columns
    // consistently for layout, scrolling, input, and selection restoration.
    constexpr std::array patches{
        GridPatch{ 0x2C46AD4, "\x48\xF7\xE1\x48\xC1\xEA\x02"sv,
            "\x48\x8B\xD1\x48\xC1\xEA\x03"sv }, // Last row for the results heading
        GridPatch{ 0x2C4720D, "\x48\xF7\xE1\x48\xC1\xEA\x02"sv,
            "\x48\x8B\xD1\x48\xC1\xEA\x03"sv }, // Mouse-wheel scroll limit
        GridPatch{ 0x2C47666, "\x48\xF7\xE2\x48\xC1\xEA\x02"sv,
            "\x48\xC1\xEA\x03"sv }, // Layout row count
        GridPatch{ 0x2C47CCC, "\x48\xF7\xE1\xC5\xF8\x57\xC0\x48\xC1\xEA\x02"sv,
            "\x48\x8B\xD1\xC5\xF8\x57\xC0\x48\xC1\xEA\x03"sv }, // Focus scroll limit
        GridPatch{ 0x2C4778D, "\xB8\x67\x66\x66\x66\x41\xF7\xEE\x44\x8B\xEA\x41\xD1\xFD\x41\x8B\xC5\xC1\xE8\x1F\x44\x03\xE8"sv,
            "\x45\x8B\xEE\x41\xC1\xED\x03"sv }, // Visible card index / 8
        GridPatch{ 0x2C47860, "\x42\x8D\x04\xAD\x00\x00\x00\x00\x41\x03\xC5"sv,
            "\x42\x8D\x04\xED\x00\x00\x00\x00"sv }, // First index of the visible row
        GridPatch{ 0x2C46DE1, "\x44\x8D\x04\x80"sv,
            "\x44\x6B\xC0\x08"sv }, // Up
        GridPatch{ 0x2C46E42, "\x43\x8D\x0C\x86\x41\x03\xC8"sv,
            "\x43\x8D\x0C\xC6"sv }, // Down candidate
        GridPatch{ 0x2C46E5C, "\xB8\x67\x66\x66\x66\x41\xF7\xE9\xD1\xFA\x8B\xC2\xC1\xE8\x1F\x03\xD0"sv,
            "\x41\x8D\x51\x07\x45\x85\xC9\x41\x0F\x49\xD1\xC1\xFA\x03"sv }, // Down row clamp
        GridPatch{ 0x2C46E72, "\x8D\x04\x92"sv,
            "\x6B\xC2\x08"sv }, // Down partial row
        GridPatch{ 0x2C46E99, "\x43\x8D\x04\x83"sv,
            "\x43\x8D\x04\xC3"sv }, // Down selected index
        GridPatch{ 0x2C46EA4, "\x44\x03\xC0"sv,
            "\x44\x8B\xC0"sv }, // Pass the index after storing the row
        GridPatch{ 0x2C46F03, "\x43\x8D\x04\x81\x44\x03\xC0"sv,
            "\x43\x8D\x04\xC1\x44\x8B\xC0"sv }, // Left from input handler
        GridPatch{ 0x2C474E6, "\x43\x8D\x04\x81\x44\x03\xC0"sv,
            "\x43\x8D\x04\xC1\x44\x8B\xC0"sv }, // Left from navigation action
        GridPatch{ 0x2C4752B, "\x41\xF7\xE9\x45\x8D\x43\x01\xD1\xFA\x8B\xC2\xC1\xE8\x1F\x03\xD0"sv,
            "\x41\x8B\xD1\xC1\xEA\x03\x45\x8D\x43\x01"sv }, // Right row clamp
        GridPatch{ 0x2C47543, "\x04"sv,
            "\x07"sv }, // Right edge comparison
        GridPatch{ 0x2C47548, "\x04\x00\x00\x00"sv,
            "\x07\x00\x00\x00"sv }, // Right edge clamp
        GridPatch{ 0x2C4754E, "\x8D\x04\x92"sv,
            "\x6B\xC2\x08"sv }, // Right partial row
        GridPatch{ 0x2C47572, "\x47\x8D\x04\x90\x45\x03\xC2"sv,
            "\x47\x8D\x04\xD0"sv }, // Right selected index
        GridPatch{ 0x2C47E21, "\xB8\x67\x66\x66\x66\xF7\xEF\xD1\xFA\x8B\xC2\xC1\xE8\x1F\x03\xD0"sv,
            "\x8D\x57\x07\x85\xFF\x0F\x49\xD7\xC1\xFA\x03"sv }, // Selected index / 8
        GridPatch{ 0x2C47E37, "\x8D\x04\x92"sv,
            "\x6B\xC2\x08"sv }, // Selected column remainder
    };
    for (const auto& patch : patches) {
        if (std::memcmp(reinterpret_cast<const void*>(base + patch.rva),
                patch.stock.data(), patch.stock.size()) != 0) {
            REX::WARN("Ultrawide Creations Menu: eight-column results skipped at RVA {:#x}: unexpected stock bytes", patch.rva);
            return;
        }
    }
    for (const auto& patch : patches) {
        std::array<unsigned char, 23> replacement;
        replacement.fill(0x90);
        std::memcpy(replacement.data(), patch.replacement.data(), patch.replacement.size());
        UpdateMarketplaceBytes(base, patch.rva, patch.stock.data(), replacement.data(), patch.stock.size(), true);
    }

    // Eight positions on a 420-unit pitch leave 240 units on each side.
    UpdateMarketplaceValue(base, 0x2C44F3B, 632.0f, 440.0f, true);
    UpdateMarketplaceValue(base, 0x2C44F46, 730.0f, 420.0f, true);
    RedirectMarketplaceConstant(base, 0x2C476FE, 0x4F04CB8, 0x4F04D84); // Origin 100 -> 240
    RedirectMarketplaceConstant(base, 0x2C476D4, 0x4F04E28, 0x4F04E14); // Long grid top 620 -> 560
}

constexpr float marketplaceHomeCardWidth = 368.0f;
constexpr float marketplaceHomeCardPitch = 400.0f;
constexpr std::uint8_t marketplaceHomeVisibleCards = 8;

void InstallCompactMarketplaceBanner(std::uintptr_t base)
{
    // The banner stays centered over the middle seven tile positions.
    constexpr int bannerCardCount = 7;
    constexpr float bannerWidth = marketplaceHomeCardWidth +
        (bannerCardCount - 1) * marketplaceHomeCardPitch;
    UpdateMarketplaceValue(base, 0x2C53854, 3640.0f, bannerWidth, true);

    // The banner draws its lower shadow separately from the artwork and overlays.
    UpdateMarketplaceValue(base, 0x2C551C6, std::array<unsigned char, 3>{ 0xFF, 0x50, 0x10 },
        std::array<unsigned char, 3>{ 0x0F, 0x1F, 0x00 }, true);

    // Move the overlays up with the bottom edge.
    constexpr float stockHeight = 900.0f;
    constexpr float compactHeight = 410.0f;
    constexpr float heightReduction = stockHeight - compactHeight;
    UpdateMarketplaceValue(base, 0x2C5385E, stockHeight, compactHeight, true);
    // Home places the first card row one row pitch below the banner origin.
    UpdateMarketplaceValue(base, 0x2C62616, 310.0f, 63.0f, true);
    UpdateMarketplaceValue(base, 0x2C6260A, 350.0f, 350.0f - heightReduction / 2.0f, true);

    // The two arrow icons share a local Y coordinate inside their hit areas.
    // Use half the hit area height so they stay on the banner's centerline.
    UpdateMarketplaceValue(base, 0x2C62B6B, std::array<unsigned char, 4>{ 0xC5, 0xF2, 0x58, 0xD5 },
        std::array<unsigned char, 4>{ 0xC5, 0xF8, 0x28, 0xD1 }, true); // xmm2 = height
    UpdateMarketplaceValue(base, 0x2C62B96,
        std::array<unsigned char, 8>{ 0xC5, 0x7A, 0x58, 0x05, 0x42, 0x20, 0x2A, 0x02 },
        std::array<unsigned char, 8>{ 0xC5, 0x78, 0x28, 0xC3, 0x0F, 0x1F, 0x40, 0x00 }, true); // xmm8 = height / 2
    // Move the arrow hit areas and their children together. The right X is
    // 3840 - 450, so the arrows sit equally far from the centered banner.
    UpdateMarketplaceValue(base, 0x2C62B54,
        std::array<unsigned char, 8>{ 0xC5, 0xFA, 0x10, 0xAB, 0xA4, 0x00, 0x00, 0x00 },
        std::array<unsigned char, 8>{ 0xC5, 0xFA, 0x10, 0x2D, 0x04, 0x20, 0x2A, 0x02 }, true); // Y offset 105 -> 25
    RedirectMarketplaceConstant(base, 0x2C62B7B, 0x4F04BF8, 0x4F04DF8); // Left center 50 -> 450
    UpdateMarketplaceValue(base, 0x2C62BDF,
        std::array<unsigned char, 12>{ 0xC5, 0xFA, 0x10, 0x0D, 0xAD, 0x23, 0x2A, 0x02, 0xC5, 0xF8, 0x28, 0xD6 },
        std::array<unsigned char, 12>{ 0xB8, 0x00, 0xE0, 0x53, 0x45, 0x66, 0x0F, 0x6E, 0xC8, 0x0F, 0x28, 0xD6 }, true); // Right center 3790 -> 3390

    // Account for the narrower banner in the lower overlay positions.
    RedirectMarketplaceConstant(base, 0x2C54EAC, 0x4F04CA4, 0x4F04E04); // Tag left offset 90 -> 500
    RedirectMarketplaceConstant(base, 0x2C631F1, 0x4F04C40, 0x4F04DFC); // Diamond right anchor 65 -> 470

    // The native loading sprite has horizontal padding. Give its ring a
    // 340x200 destination so it looks round and stays inside the banner.
    RedirectMarketplaceConstant(base, 0x2C5487F, 0x4F04D8C, 0x4F04CB8); // Loading Y half-size 250 -> 100
    RedirectMarketplaceConstant(base, 0x2C5494D, 0x4F04DE8, 0x4F04D40); // Loading X half-size 425 -> 170
    RedirectMarketplaceConstant(base, 0x2C54A3F, 0x4F04E68, 0x4F04DBC); // Loading width 850 -> 340
    RedirectMarketplaceConstant(base, 0x2C54A47, 0x4F04E04, 0x4F04D68); // Loading height 500 -> 200

    constexpr float titleRight = 2700.0f;
    UpdateMarketplaceValue(base, 0x4F062E0, std::array{ 75.0f, 470.0f, 3675.0f, 750.0f },
        std::array{ 75.0f, 0.0f, titleRight, 263.0f }, false);
    UpdateMarketplaceValue(base, 0x4F062F0, std::array{ 75.0f, 600.0f, 3675.0f, 750.0f },
        std::array{ 75.0f, 115.0f, titleRight, 263.0f }, false);

    struct BannerOffset
    {
        std::int32_t operand;
        std::int32_t stock;
        std::int32_t compact;
    };
    constexpr std::array bannerOffsets{
        BannerOffset{ 0x2C539D8, 0x4F04E20, 0x4F04CD8 },  // 600 -> 115
        BannerOffset{ 0x2C53ABC, 0x4F04DFC, 0x4F04DF0 },  // 470 -> 0
        BannerOffset{ 0x2C53B09, 0x4F04E4C, 0x4F04D98 },  // 750 -> 263
        BannerOffset{ 0x2C53B77, 0x4F04E50, 0x4F04D98 },
        BannerOffset{ 0x2C53BE5, 0x4F04E54, 0x4F04D98 },
        BannerOffset{ 0x2C53CFF, 0x4F04E44, 0x4F04D98 },
        BannerOffset{ 0x2C53D65, 0x4F04E40, 0x4F04D98 },
        BannerOffset{ 0x2C53AFB, 0x4F04F8C, 0x4F04F4C },  // Title right edge
        BannerOffset{ 0x2C53B69, 0x4F04F88, 0x4F04F4C },
        BannerOffset{ 0x2C53BD7, 0x4F04F84, 0x4F04F4C },
        BannerOffset{ 0x2C53C3D, 0x4F04F80, 0x4F04F4C },
        BannerOffset{ 0x2C53C9B, 0x4F04F7C, 0x4F04F4C },
    };
    for (const auto& offset : bannerOffsets) {
        RedirectMarketplaceConstant(base, offset.operand, offset.stock, offset.compact);
    }

    constexpr std::array titleFontOperands{
        0x2C53A0B, 0x2C53B32, 0x2C53BA0, 0x2C53C0E, 0x2C53C6C,
        0x2C53CCA, 0x2C53D28, 0x2C53D8E, 0x2C53DDC
    };
    for (const auto operand : titleFontOperands) {
        UpdateMarketplaceValue(base, operand, std::uint32_t{138}, std::uint32_t{84}, true);
    }

    // The creator byline has a separate baseline and font from the title.
    RedirectMarketplaceConstant(base, 0x2C5412D, 0x4F04E48, 0x4F04D98); // 748 -> 263
    UpdateMarketplaceValue(base, 0x2C543B3, std::uint32_t{70}, std::uint32_t{47}, true);
}

using MarketplaceRowDraw = void (*)(void*);
MarketplaceRowDraw drawMarketplaceRow = nullptr;
const float* marketplaceLayoutScale = nullptr;
const std::int32_t* marketplaceViewportHeight = nullptr;

using MarketplaceArtworkReady = bool (*)(std::uint64_t);
using MarketplaceArtworkLookup = const void* (*)(std::uint64_t, bool);
using MarketplaceImageDraw = void (*)(void*);
using MarketplaceDrawMatrix = std::array<float, 8>;
using SetMarketplaceDrawMatrix = void (*)(const MarketplaceDrawMatrix*);
using FlushMarketplaceDraw = void (*)(void*);
MarketplaceArtworkReady isMarketplaceArtworkReady = nullptr;
MarketplaceArtworkLookup lookupMarketplaceArtwork = nullptr;
MarketplaceImageDraw drawMarketplaceImage = nullptr;
SetMarketplaceDrawMatrix setMarketplaceDrawMatrix = nullptr;
FlushMarketplaceDraw flushMarketplaceDraw = nullptr;
void* const* marketplaceRenderer = nullptr;
std::uint32_t* marketplaceDrawMode = nullptr;
const void* const* marketplaceArtworkCache = nullptr;

struct MarketplaceStaticImage
{
    std::uint32_t id;
    std::byte padding[0x1C];
    const void* texture;
    const MarketplaceStaticImage* next;
};
const MarketplaceStaticImage* const* marketplaceStaticImages = nullptr;

template <class T>
T ReadMarketplaceField(const std::byte* object, std::size_t offset)
{
    T value{};
    std::memcpy(&value, object + offset, sizeof(value));
    return value;
}

template <class T>
void WriteMarketplaceField(std::byte* object, std::size_t offset, const T& value)
{
    std::memcpy(object + offset, &value, sizeof(value));
}

using MarketplaceRect = std::array<float, 4>;

void SetMarketplaceWidgetRect(std::byte* widget, const MarketplaceRect& rect)
{
    using SetRect = void (*)(void*, const MarketplaceRect*);
    const auto* vtable = ReadMarketplaceField<const std::byte*>(widget, 0);
    ReadMarketplaceField<SetRect>(vtable, 0x40)(widget, &rect);
}

constexpr float marketplaceRibbonHeight = 56.0f;
constexpr float marketplaceBundleBadgeWidth = 190.0f;

bool HasMarketplaceCardSaleRibbon(const std::byte* tile, const std::byte* data,
    const MarketplaceRect& artwork)
{
    if (!data || ReadMarketplaceField<bool>(data, 0xFB) || !ReadMarketplaceField<bool>(tile, 0x18D)) {
        return false;
    }
    const auto* ribbon = ReadMarketplaceField<const std::byte*>(tile, 0x108);
    const float originalPrice = ReadMarketplaceField<float>(ribbon, 0x88);
    const float salePrice = ReadMarketplaceField<float>(ribbon, 0x8C);
    const auto [left, top, right, bottom] = artwork;
    return salePrice >= 0.0f && salePrice < originalPrice &&
        right > left && bottom >= top + marketplaceRibbonHeight;
}

void DrawMarketplaceWidget(std::byte* widget)
{
    const auto* vtable = ReadMarketplaceField<const std::byte*>(widget, 0);
    ReadMarketplaceField<MarketplaceImageDraw>(vtable, 0x10)(widget);
}

void SetMarketplaceCardMatrix(const MarketplaceDrawMatrix& matrix)
{
    // Match the game's card draw order when changing the renderer transform.
    if (*marketplaceDrawMode == 1) {
        flushMarketplaceDraw(*marketplaceRenderer);
    }
    *marketplaceDrawMode = 0;
    setMarketplaceDrawMatrix(&matrix);
}

void DrawTranslatedMarketplaceLabel(std::byte* label, const MarketplaceDrawMatrix& matrix,
    float offsetX, float offsetY)
{
    // Keep the original text rectangle and Scaleform resource intact.
    // Translate just this draw using the same matrix as the native card.
    auto translated = matrix;
    translated[3] += matrix[0] * offsetX;
    translated[7] += matrix[5] * offsetY;
    SetMarketplaceCardMatrix(translated);
    DrawMarketplaceWidget(label);
    SetMarketplaceCardMatrix(matrix);
}

void DrawMarketplaceCardSale(const std::byte* tile, const MarketplaceDrawMatrix& matrix)
{
    const auto* data = ReadMarketplaceField<const std::byte*>(tile, 0x70);
    auto* ribbon = ReadMarketplaceField<std::byte*>(tile, 0x108);
    const auto* image = ReadMarketplaceField<const std::byte*>(tile, 0x110);
    const auto artwork = ReadMarketplaceField<MarketplaceRect>(image, 0x20);
    if (HasMarketplaceCardSaleRibbon(tile, data, artwork)) {
        const auto [left, top, right, bottom] = artwork;
        // The sale group has separate diagonal stripe and green ribbon images.
        // Use the green ribbon for every card with a lower sale price.
        auto* greenRibbon = ReadMarketplaceField<std::byte*>(ribbon, 0x80);
        auto* label = ReadMarketplaceField<std::byte*>(ribbon, 0x78);
        constexpr std::size_t visibilityOffset = 0x10;
        const bool ribbonVisible = ReadMarketplaceField<bool>(greenRibbon, visibilityOffset);
        const bool labelVisible = ReadMarketplaceField<bool>(label, visibilityOffset);
        WriteMarketplaceField(greenRibbon, visibilityOffset, true);
        WriteMarketplaceField(label, visibilityOffset, true);
        SetMarketplaceWidgetRect(greenRibbon, { left, top, right, top + marketplaceRibbonHeight });
        DrawMarketplaceWidget(greenRibbon);

        const auto bounds = ReadMarketplaceField<MarketplaceRect>(label, 0x20);
        const float shiftX = (left + right - bounds[0] - bounds[2]) * 0.5f;
        DrawTranslatedMarketplaceLabel(label, matrix, shiftX, top + 7.0f - bounds[1]);
        WriteMarketplaceField(label, visibilityOffset, labelVisible);
        WriteMarketplaceField(greenRibbon, visibilityOffset, ribbonVisible);
    } else {
        DrawMarketplaceWidget(ribbon);
    }
    DrawMarketplaceWidget(ReadMarketplaceField<std::byte*>(tile, 0xE8));
}

const void* FindMarketplaceBlackBackdrop()
{
    constexpr std::uint32_t backdropId = 0x1D;
    for (auto* asset = *marketplaceStaticImages; asset; asset = asset->next) {
        if (asset->id == backdropId) {
            return asset->texture;
        }
    }
    return nullptr;
}

void DrawMarketplaceBundleBadge(const std::byte* tile, const MarketplaceDrawMatrix& matrix)
{
    const auto* data = ReadMarketplaceField<const std::byte*>(tile, 0x70);
    if (!data || !ReadMarketplaceField<bool>(data, 0x109)) {
        return;
    }
    auto* image = ReadMarketplaceField<std::byte*>(tile, 0x110);
    const auto artwork = ReadMarketplaceField<MarketplaceRect>(image, 0x20);
    const auto [left, top, right, bottom] = artwork;
    const float badgeTop = top + (HasMarketplaceCardSaleRibbon(tile, data, artwork) ? marketplaceRibbonHeight : 0.0f);
    const auto* blackTexture = FindMarketplaceBlackBackdrop();
    if (blackTexture && right >= left + marketplaceBundleBadgeWidth && bottom >= badgeTop + marketplaceRibbonHeight) {
        // Keep the dark backing with the badge and clear of the sale ribbon.
        constexpr std::size_t alphaOffset = 0xC0;
        constexpr std::size_t textureOffset = 0xD0;
        const float alpha = ReadMarketplaceField<float>(image, alphaOffset);
        const auto* texture = ReadMarketplaceField<const void*>(image, textureOffset);
        WriteMarketplaceField(image, 0x20,
            MarketplaceRect{ left, badgeTop, left + marketplaceBundleBadgeWidth, badgeTop + marketplaceRibbonHeight });
        WriteMarketplaceField(image, alphaOffset, alpha * 0.75f);
        WriteMarketplaceField(image, textureOffset, blackTexture);
        drawMarketplaceImage(image);
        WriteMarketplaceField(image, textureOffset, texture);
        WriteMarketplaceField(image, alphaOffset, alpha);
        WriteMarketplaceField(image, 0x20, artwork);
    }
    auto* icon = ReadMarketplaceField<std::byte*>(tile, 0x130);
    auto* label = ReadMarketplaceField<std::byte*>(tile, 0xF0);
    SetMarketplaceWidgetRect(icon, { left + 8.0f, badgeTop + 11.0f, left + 42.0f, badgeTop + 45.0f });
    DrawMarketplaceWidget(icon);
    const auto bounds = ReadMarketplaceField<MarketplaceRect>(label, 0x20);
    DrawTranslatedMarketplaceLabel(label, matrix, left + 48.0f - bounds[0], badgeTop + 7.0f - bounds[1]);
}

template <std::size_t handleOffset, bool banner>
void DrawCurrentMarketplaceArtwork(const std::byte* tile)
{
    const auto* data = ReadMarketplaceField<const std::byte*>(tile, 0x70);
    auto* image = ReadMarketplaceField<std::byte*>(tile, 0x110);
    if (!data || !image || !*marketplaceArtworkCache) {
        return;
    }

    const auto artworkHandle = ReadMarketplaceField<std::uint64_t>(data, handleOffset);
    const auto* assignedTexture = ReadMarketplaceField<const void*>(image, 0xD0);
    // Layout stores a borrowed texture. Looking up another tile can evict it
    // before this tile is drawn. Compare it with the current cache entry before
    // calling the renderer. The separately registered loading backdrop can
    // cover the full slot, while the spinner keeps its round proportions.
    const bool ready = isMarketplaceArtworkReady(artworkHandle);
    const void* blackTexture = nullptr;
    if constexpr (!banner) {
        if (!ready) {
            blackTexture = FindMarketplaceBlackBackdrop();
        }
    }
    const auto* currentTexture = ready ? lookupMarketplaceArtwork(artworkHandle, banner) : blackTexture;
    if (assignedTexture && assignedTexture == currentTexture) {
        drawMarketplaceImage(image);
    }
}

void InstallMarketplaceArtworkDrawGuard(std::uintptr_t base)
{
    auto& trampoline = REL::GetTrampoline();
    if (trampoline.free_size() < 4 * sizeof(std::uintptr_t)) {
        REX::WARN("Ultrawide Creations Menu: Creations artwork draw guard unavailable");
        return;
    }
    isMarketplaceArtworkReady = reinterpret_cast<MarketplaceArtworkReady>(base + 0x2C29E20);
    lookupMarketplaceArtwork = reinterpret_cast<MarketplaceArtworkLookup>(base + 0x2C29D30);
    drawMarketplaceImage = reinterpret_cast<MarketplaceImageDraw>(base + 0x2C38870);
    setMarketplaceDrawMatrix = reinterpret_cast<SetMarketplaceDrawMatrix>(base + 0x2C27240);
    flushMarketplaceDraw = reinterpret_cast<FlushMarketplaceDraw>(base + 0x320F690);
    marketplaceRenderer = reinterpret_cast<void* const*>(base + 0x5E7B9B8);
    marketplaceDrawMode = reinterpret_cast<std::uint32_t*>(base + 0x5E7B9AC);
    marketplaceArtworkCache = reinterpret_cast<const void* const*>(base + 0x5FD40C8);
    marketplaceStaticImages = reinterpret_cast<const MarketplaceStaticImage* const*>(base + 0x5FD4078);

    const auto installDrawHook = [&]<std::size_t size>(std::uintptr_t rva,
                                     const std::array<unsigned char, size>& stock, auto draw) {
        // Pass the tile in RBX to the hook instead of dispatching the child.
        std::array<unsigned char, size> replacement{};
        replacement.fill(0x90);
        constexpr std::array<unsigned char, 3> tileArgument{ 0x48, 0x8B, 0xCB };
        constexpr std::array<unsigned char, 5> matrixArgument{ 0x48, 0x8D, 0x54, 0x24, 0x20 };
        constexpr bool passMatrix =
            std::is_invocable_v<decltype(draw), const std::byte*, const MarketplaceDrawMatrix&>;
        auto* call = std::ranges::copy(tileArgument, replacement.data()).out;
        if constexpr (passMatrix) {
            // The native card keeps the unscaled matrix at RSP + 0x20.
            call = std::ranges::copy(matrixArgument, call).out;
        }
        *call++ = 0xFF;
        *call++ = 0x15;
        const auto endOfCall = (call - replacement.data()) + sizeof(std::int32_t);
        const auto slot = trampoline.allocate_branch6(reinterpret_cast<std::uintptr_t>(draw));
        const auto distance = static_cast<std::intptr_t>(slot) - static_cast<std::intptr_t>(base + rva + endOfCall);
        if (!std::in_range<std::int32_t>(distance)) {
            REX::WARN("Ultrawide Creations Menu: Creations draw hook is out of range at RVA {:#x}", rva);
            return;
        }
        const auto displacement = static_cast<std::int32_t>(distance);
        std::memcpy(call, &displacement, sizeof(displacement));
        UpdateMarketplaceValue(base, rva, stock, replacement, true);
    };

    constexpr std::array<unsigned char, 13> stockArtworkDraw{
        0x48, 0x8B, 0x8B, 0x10, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x10
    };
    installDrawHook(0x2C551C9, stockArtworkDraw, &DrawCurrentMarketplaceArtwork<0x128, true>);
    installDrawHook(0x2C5967C, stockArtworkDraw, &DrawCurrentMarketplaceArtwork<0x120, false>);

    constexpr std::array<unsigned char, 26> stockSaleDraw{
        0x48, 0x8B, 0x8B, 0x08, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x10,
        0x48, 0x8B, 0x8B, 0xE8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x10
    };
    installDrawHook(0x2C596FA, stockSaleDraw, &DrawMarketplaceCardSale);

    // The stock card sets this flag from the promotion type even when it
    // already has a sale price. Enable native percentage text preparation
    // in the card's sale initialization path, including for discounted bundles.
    UpdateMarketplaceValue(base, 0x2C583D8,
        std::array<unsigned char, 13>{ 0x0F, 0xB6, 0x80, 0xF7, 0x00, 0x00, 0x00, 0x88, 0x81, 0x90, 0x00, 0x00, 0x00 },
        std::array<unsigned char, 13>{ 0xC6, 0x81, 0x90, 0x00, 0x00, 0x00, 0x01, 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, true);

    constexpr std::array<unsigned char, 26> stockBundleDraw{
        0x48, 0x8B, 0x8B, 0x30, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x10,
        0x48, 0x8B, 0x8B, 0xF0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x10
    };
    installDrawHook(0x2C5972E, stockBundleDraw, &DrawMarketplaceBundleBadge);
}

void DrawMarketplaceRowAboveFooter(void* row)
{
    constexpr std::size_t rowTopOffset = 0x24;
    constexpr float footerClearance = 150.0f;
    float top = 0.0f;
    std::memcpy(&top, static_cast<const std::byte*>(row) + rowTopOffset, sizeof(top));

    const float scale = *marketplaceLayoutScale;
    const auto height = *marketplaceViewportHeight;
    if (scale > 0.0f && height > 0 && top >= static_cast<float>(height) / scale - footerClearance) {
        return;
    }
    drawMarketplaceRow(row);
}

void InstallCompactMarketplaceHomeRows(std::uintptr_t base)
{
    // Match the carousel's row bounds to its reduced card artwork and footer.
    // Two rows must fit between the banner and the command footer at 3440x1440:
    // the first row sits one row pitch below the banner origin, so each pitch
    // unit moves row 2 twice. Give the text the height freed by the artwork.
    constexpr float artworkWidth = marketplaceHomeCardWidth;
    constexpr float artworkHeight = 182.0f;
    UpdateMarketplaceValue(base, 0x2C31AB9, 730.0f, 428.0f, true);  // Home row pitch
    UpdateMarketplaceValue(base, 0x2C4857A, 200.0f, 65.0f, true);   // Heading to cards
    // Center nine Home tiles, including View All, with clear edge lanes for arrows.
    RedirectMarketplaceConstant(base, 0x2C48555, 0x4F04E38, 0x4F04DE0); // Pitch 730 -> 400
    RedirectMarketplaceConstant(base, 0x2C48566, 0x4F053C8, 0x4F05398); // Offset -270 -> -80
    // Exclude card centers outside the viewport so adjacent tiles stay out of the arrow lanes.
    UpdateMarketplaceValue(base, 0x2C4932E, std::array<unsigned char, 5>{0xC4, 0xC1, 0x62, 0x58, 0xE3},
        std::array<unsigned char, 5>{0xC4, 0xC1, 0x78, 0x28, 0xE3}, true);
    UpdateMarketplaceValue(base, 0x2C49353, std::array<unsigned char, 4>{0xC5, 0xE0, 0x57, 0xD7},
        std::array<unsigned char, 4>{0xC5, 0xE8, 0x57, 0xD2}, true);

    // These writes initialize the two title lines used by the native splitter.
    for (const auto operand : {0x2C584E4, 0x2C58579}) {
        UpdateMarketplaceValue(base, operand, 50.0f, 39.0f, true);
    }
    RedirectMarketplaceConstant(base, 0x2C5A073, 0x4F04AD8, 0x4F04A40); // Title inset 15 -> 8

    // The card update places the localized bundle caption next to the icon.
    RedirectMarketplaceConstant(base, 0x2C57D27, 0x4F04C0C, 0x4F04BC0); // Bundle label font 53 -> 35
    UpdateMarketplaceValue(base, 0x2C57D53, 53.0f, 35.0f, true);

    // The ring occupies a narrow part of the sprite texture, so use a
    // centered 180x100 box to keep it round and clear of neighboring cards.
    UpdateMarketplaceValue(base, 0x2C59B9E,
        std::array<unsigned char, 9>{ 0xC4, 0xC1, 0x7A, 0x10, 0x9F, 0xAC, 0x00, 0x00, 0x00 },
        std::array<unsigned char, 9>{ 0xC5, 0xFA, 0x10, 0x1D, 0x12, 0xB1, 0x2A, 0x02, 0x90 }, true); // Loading height = 100
    RedirectMarketplaceConstant(base, 0x2C59BD5, 0x4F04DB8, 0x4F04CA4); // Loading X half-size 333 -> 90
    RedirectMarketplaceConstant(base, 0x2C59BDD, 0x4F04E2C, 0x4F04D54); // Loading width 666 -> 180

    // The shadow is a separate image under each card and its focus border.
    UpdateMarketplaceValue(base, 0x2C59679, std::array<unsigned char, 3>{ 0xFF, 0x50, 0x10 },
        std::array<unsigned char, 3>{ 0x0F, 0x1F, 0x00 }, true);

    // Cull only the draw of rows whose heading starts within the footer area.
    // The stock update, paging, and input handlers can still scroll them into view.
    constexpr std::uintptr_t rowDrawSlot = 0x4EFD098;
    drawMarketplaceRow = reinterpret_cast<MarketplaceRowDraw>(base + 0x2C49080);
    constexpr std::uintptr_t marketplaceScaleRva = 0x592F9D0;
    marketplaceLayoutScale = reinterpret_cast<const float*>(base + marketplaceScaleRva);
    marketplaceViewportHeight = reinterpret_cast<const std::int32_t*>(base + 0x592F9BC);
    UpdateMarketplaceValue(base, rowDrawSlot, drawMarketplaceRow, &DrawMarketplaceRowAboveFooter, false);

    // The native two-line title splitter measures against a fixed stock width.
    for (const auto operand : {0x2C58668, 0x2C58C93}) {
        RedirectMarketplaceConstant(base, operand, 0x4F04E30, 0x4F04DBC); // 695 -> 340
    }
    // Row, arrow, and clip height must cover the artwork, both text lines, and focus growth.
    RedirectMarketplaceConstant(base, 0x2C4879D, 0x4F04E14, 0x4F04DD8); // 560 -> 375
    // Move both arrow icons and hit areas up by 88 units. The native icon
    // anchor uses the sum of card inset and row height (65 + 375).
    RedirectMarketplaceConstant(base, 0x2C494D4, 0x4F0433C, 0x4F04250); // 0.5 -> 0.3
    RedirectMarketplaceConstant(base, 0x2C494BC, 0x4F04CE4, 0x4F04BB0); // Hit top offset 120 -> 32
    for (const auto operand : { 0x2C49510, 0x2C49595 }) {
        RedirectMarketplaceConstant(base, operand, 0x4F04C84, 0x4F04D3C); // Hit bottom inset 80 -> 168
    }
    UpdateMarketplaceValue(base, 0x2C57F45, 715.0f, artworkWidth, true);
    UpdateMarketplaceValue(base, 0x2C57F4F, 402.0f, artworkHeight, true);
    UpdateMarketplaceValue(base, 0x2C4984A, 715.0f, artworkWidth, true);
    UpdateMarketplaceValue(base, 0x2C49854, 402.0f, artworkHeight, true); // View All

    // Keep the carousel's visible count, selection wrap, and paging in sync.
    constexpr auto visibleCards = marketplaceHomeVisibleCards;
    constexpr std::array visibleCountOperands{
        0x2C48997, 0x2C48C38, 0x2C48C47, 0x2C48C52, 0x2C48C55,
        0x2C48CE2, 0x2C48EC2, 0x2C49193, 0x2C49469
    };
    for (const auto operand : visibleCountOperands) {
        UpdateMarketplaceValue(base, operand, std::uint8_t{5}, visibleCards, true);
    }
    UpdateMarketplaceValue(base, 0x2C48FBA, std::uint8_t{4}, std::uint8_t{visibleCards - 1}, true);
    UpdateMarketplaceValue(base, 0x2C48FC4, std::uint32_t{4}, std::uint32_t{visibleCards - 1}, true);
    UpdateMarketplaceValue(base, 0x2C49031, std::uint8_t{4}, std::uint8_t{visibleCards - 1}, true);
    UpdateMarketplaceValue(base, 0x2C4903F, std::uint32_t{4}, std::uint32_t{visibleCards - 1}, true);
    // Moving focus between Home rows keeps the on-screen card slot and clamps
    // it to the target row only when that row is shorter than the visible count.
    constexpr std::array rowChangeClampOperands{ 0x2C2F625, 0x2C2F880 }; // Up, Down
    for (const auto operand : rowChangeClampOperands) {
        UpdateMarketplaceValue(base, operand, std::uint8_t{5}, visibleCards, true);
    }

    constexpr std::array footerHeightOperands{ 0x2C5993C, 0x2C59E0C, 0x2C6DFDF };
    for (const auto operand : footerHeightOperands) {
        RedirectMarketplaceConstant(base, operand, 0x4F04D70, 0x4F04D48); // 210 -> 175
    }
    constexpr std::array footerLineOperands{ 0x2C5A07B, 0x2C5A0C7 };
    for (const auto operand : footerLineOperands) {
        RedirectMarketplaceConstant(base, operand, 0x4F04C40, 0x4F04C10); // 65 -> 55
    }
    // The same formatter sets the font for Owned, regular, and both sale prices.
    constexpr std::array priceFontOperands{ 0x2C581AA, 0x2C5823F, 0x2C582F9, 0x2C58366 };
    for (const auto operand : priceFontOperands) {
        UpdateMarketplaceValue(base, operand, std::uint32_t{53}, std::uint32_t{40}, true);
    }
    // Keep the icon size and the space reserved next to the text in sync.
    RedirectMarketplaceConstant(base, 0x2C5A178, 0x4F04C30, 0x4F04BEC); // Credit size 60 -> 44
    RedirectMarketplaceConstant(base, 0x2C5A1D5, 0x4F04C30, 0x4F04BEC);
    // Both badge variants use the same 45x45 draw bounds. The right edge is
    // 50 units from the left anchor, and the top is 5 units above the baseline.
    for (const auto operand : { 0x2C5A290, 0x2C5A34E }) {
        RedirectMarketplaceConstant(base, operand, 0x4F04B1C, 0x4F0531C); // Left extension 20 -> -5
    }
    for (const auto operand : { 0x2C5A2AA, 0x2C5A368 }) {
        RedirectMarketplaceConstant(base, operand, 0x4F04C40, 0x4F04BE0); // Bottom offset 65 -> 40
    }
    // The price line needs room below the title line inside the card frame.
    RedirectMarketplaceConstant(base, 0x2C5A0FE, 0x4F04D78, 0x4F04D48); // Price text lower bound 220 -> 175

    // Keep focus visible without letting the enlarged card cover the next row.
    constexpr std::array focusScaleOperands{ 0x2C59ACE, 0x2C6E148, 0x2C6E3DB };
    for (const auto operand : focusScaleOperands) {
        RedirectMarketplaceConstant(base, operand, 0x4F0424C, 0x4F04050); // 30% -> 7%
    }
    RedirectMarketplaceConstant(base, 0x2C59AD6, 0x4F04C84, 0x4F04B1C); // Focus rise 80 -> 20
}

void InstallCompactMarketplaceDialogs(std::uintptr_t base)
{
    // The notice background is sized from the text's measured height, but
    // centers against a separate copy of the stock text box height.
    constexpr std::uint32_t noticeTextBoxHeight = 373;
    constexpr std::uint32_t noticeAnchorY = 890;
    UpdateMarketplaceValue(base, 0x2C3340D, 72.0f, 37.5f, true);
    UpdateMarketplaceValue(base, 0x4F062C0, std::array{ 0.0f, 0.0f, 2540.0f, 710.0f },
        std::array{ 0.0f, 0.0f, 1329.0f, static_cast<float>(noticeTextBoxHeight) }, false);
    UpdateMarketplaceValue(base, 0x2C33AE6, std::uint32_t{710}, noticeTextBoxHeight, true);
    RedirectMarketplaceConstant(base, 0x2C33AAD, 0x4F04F40, 0x4F04ECC); // 2640 -> 1395
    UpdateMarketplaceValue(base, 0x2C33AB3, std::uint8_t{100}, std::uint8_t{63}, true);
    RedirectMarketplaceConstant(base, 0x2C33432, 0x4F04E88, 0x4F04E6C); // 945 -> 890
    UpdateMarketplaceValue(base, 0x2C33AFB, std::uint32_t{945}, noticeAnchorY, true);

    // Use the native button rectangles so mouse and controller selection
    // follow the compact layout.
    UpdateMarketplaceValue(base, 0x4F06460, std::array{ 720.0f, 330.0f, 3120.0f, 1830.0f },
        std::array{ 925.0f, 330.0f, 2915.0f, 1180.0f }, false);
    UpdateMarketplaceValue(base, 0x4F06280, std::array{ 720.0f, 330.0f, 3120.0f, 480.0f },
        std::array{ 925.0f, 330.0f, 2915.0f, 450.0f }, false);
    UpdateMarketplaceValue(base, 0x4F062A0, std::array{ 750.0f, 360.0f, 3100.0f, 510.0f },
        std::array{ 945.0f, 350.0f, 2900.0f, 450.0f }, false);
    UpdateMarketplaceValue(base, 0x4F062B0, std::array{ 740.0f, 500.0f, 3090.0f, 610.0f },
        std::array{ 945.0f, 470.0f, 2900.0f, 550.0f }, false);
    UpdateMarketplaceValue(base, 0x4F062D0, std::array{ 740.0f, 640.0f, 3090.0f, 750.0f },
        std::array{ 945.0f, 570.0f, 2900.0f, 650.0f }, false);
    UpdateMarketplaceValue(base, 0x4F06300, std::array{ 740.0f, 780.0f, 3090.0f, 890.0f },
        std::array{ 945.0f, 666.0f, 2900.0f, 746.0f }, false);
    RedirectMarketplaceConstant(base, 0x2C5BE11, 0x4F04E60, 0x4F04E2C); // 780 -> 666
    RedirectMarketplaceConstant(base, 0x2C5BE19, 0x4F04E6C, 0x4F04E40); // 890 -> 746
    RedirectMarketplaceConstant(base, 0x2C5C039, 0x4F04D14, 0x4F04CB8); // 140 -> 100
    RedirectMarketplaceConstant(base, 0x2C5C05C, 0x4F04E3C, 0x4F04E88); // 740 -> 945
    RedirectMarketplaceConstant(base, 0x2C5C070, 0x4F04F64, 0x4F04F54); // 3090 -> 2900
    constexpr std::array optionFontOperands{
        0x2C5B736, 0x2C5B9AC, 0x2C5BD95, 0x2C5BFFE, 0x2C5C211, 0x2C5C3CD, 0x2C5C578
    };
    for (const auto operand : optionFontOperands) {
        UpdateMarketplaceValue(base, operand, 72.0f, 49.0f, true);
    }
    // The button subtracts vertical padding from both ends of the text box.
    // Recover 20 units of label height inside the existing compact rows.
    constexpr std::array optionVerticalPaddingOperands{
        0x2C5B7DC, 0x2C5BB6B, 0x2C5BE29, 0x2C5C051, 0x2C5C25C, 0x2C5C418, 0x2C5C5B9
    };
    for (const auto operand : optionVerticalPaddingOperands) {
        UpdateMarketplaceValue(base, operand, 20.0f, 10.0f, true);
    }
    UpdateMarketplaceValue(base, 0x2C5B453, 90.0f, 65.0f, true);
}

constexpr float minimumUltrawideAspect = 2.2f;
constexpr std::uintptr_t mainSceneOwnerRva = 0x5FD9B78;
constexpr std::uintptr_t marketplaceResizeRva = 0x13C58B0;
constexpr std::uintptr_t marketplaceStartupRva = 0x2C32120;
std::uintptr_t marketplaceModule = 0;

struct ViewportSize
{
    std::int32_t width = 0;
    std::int32_t height = 0;
};

BOOL CALLBACK FindLargestProcessWindow(HWND window, LPARAM parameter)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId() || !IsWindowVisible(window)) {
        return TRUE;
    }

    RECT client{};
    if (!GetClientRect(window, &client)) {
        return TRUE;
    }
    const ViewportSize candidate{ client.right - client.left, client.bottom - client.top };
    auto& largest = *reinterpret_cast<ViewportSize*>(parameter);
    if (candidate.width > 0 && candidate.height > 0 &&
        static_cast<std::int64_t>(candidate.width) * candidate.height >
            static_cast<std::int64_t>(largest.width) * largest.height) {
        largest = candidate;
    }
    return TRUE;
}

ViewportSize GetUltrawideViewport()
{
    // On the first trip into Creations, Native Ultrawide Menus can widen the
    // game scene after Marketplace begins initializing. The game window is
    // already final at that point, so use it to break that startup cycle.
    ViewportSize windowSize{};
    EnumWindows(&FindLargestProcessWindow, reinterpret_cast<LPARAM>(&windowSize));

    const auto* scene = *reinterpret_cast<const std::byte* const*>(marketplaceModule + mainSceneOwnerRva);
    if (scene) {
        const ViewportSize sceneSize{
            ReadMarketplaceField<std::int32_t>(scene, 0x38),
            ReadMarketplaceField<std::int32_t>(scene, 0x34)
        };
        if (sceneSize.width > 0 && sceneSize.height > 0 &&
            static_cast<float>(sceneSize.width) / sceneSize.height >= minimumUltrawideAspect) {
            return sceneSize;
        }
    }
    if (windowSize.width > 0 && windowSize.height > 0 &&
        static_cast<float>(windowSize.width) / windowSize.height >= minimumUltrawideAspect) {
        return windowSize;
    }

    // SFSE's post-load event occurs before Starfield necessarily exposes a
    // visible window. Native Ultrawide Menus selects its aspect from the same
    // desktop, so this lets us install the Creations constants before the
    // first disclaimer movie is constructed.
    return { GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

void PrepareUltrawideCreations()
{
    static std::atomic_bool installed = false;
    if (installed.load()) {
        return;
    }

    if (!GetModuleHandleW(L"NativeUltrawideMenus.dll")) {
        [[maybe_unused]] static const bool reported = [] {
            REX::WARN("Ultrawide Creations Menu requires Native Ultrawide Menus");
            return true;
        }();
        return;
    }

    const auto [width, height] = GetUltrawideViewport();
    if (width <= 0 || height <= 0 || static_cast<float>(width) / height < minimumUltrawideAspect) {
        return;
    }

    static std::once_flag install;
    std::call_once(install, [width, height] {
        // Remove the 16:9 cap only inside the storefront's resize method.
        // The native path then calculates scale and pointer offsets from its
        // own viewport, leaving the shared aspect constant untouched.
        constexpr std::uintptr_t capBranch = 0x13C59D3;
        constexpr std::array<unsigned char, 2> stock{ 0x77, 0x15 };
        constexpr std::array<unsigned char, 2> uncapped{ 0x66, 0x90 };
        const auto* site = reinterpret_cast<const void*>(marketplaceModule + capBranch);
        if (std::memcmp(site, stock.data(), stock.size()) != 0) {
            REX::WARN("Ultrawide Creations Menu: unexpected storefront viewport branch");
            return;
        }
        UpdateMarketplaceValue(marketplaceModule, capBranch, stock, uncapped, true);
        if (std::memcmp(site, uncapped.data(), uncapped.size()) != 0) {
            return;
        }

        const float aspect = static_cast<float>(width) / height;
        InstallMarketplaceDetailBounds(marketplaceModule, aspect);
        InstallMarketplaceResultsHeading(marketplaceModule);
        InstallCompactMarketplaceResultsGrid(marketplaceModule);
        InstallCompactMarketplaceBanner(marketplaceModule);
        InstallCompactMarketplaceHomeRows(marketplaceModule);
        InstallMarketplaceArtworkDrawGuard(marketplaceModule);
        InstallCompactMarketplaceDialogs(marketplaceModule);
        installed.store(true);
        REX::INFO("Ultrawide Creations Menu: enabled for {}x{} game render area", width, height);
    });
}

template <std::uintptr_t target>
void PrepareAndRunMarketplace()
{
    PrepareUltrawideCreations();
    reinterpret_cast<void (*)()>(marketplaceModule + target)();
}

bool InstallMarketplaceEntryHooks()
{
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!base) {
        return false;
    }

    // Cover the storefront's viewport callback and its first native update.
    // Both original calls are argument-free and stay in their normal order.
    constexpr std::uintptr_t resizeCall = 0x13C5B5B;
    constexpr std::uintptr_t startupCall = 0x13C5C25;
    for (const auto& [address, target] : {
             std::pair{ resizeCall, marketplaceResizeRva }, std::pair{ startupCall, marketplaceStartupRva } }) {
        const auto* site = reinterpret_cast<const std::byte*>(base + address);
        const auto destination = static_cast<std::intptr_t>(address + 5) + ReadMarketplaceField<std::int32_t>(site, 1);
        if (ReadMarketplaceField<unsigned char>(site, 0) != 0xE8 || destination != target) {
            REX::WARN("Ultrawide Creations Menu: unexpected storefront call at RVA {:#x}", address);
            return false;
        }
    }

    marketplaceModule = base;
    auto& trampoline = REL::GetTrampoline();
    trampoline.write_call<5>(base + resizeCall, &PrepareAndRunMarketplace<marketplaceResizeRva>);
    trampoline.write_call<5>(base + startupCall, &PrepareAndRunMarketplace<marketplaceStartupRva>);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(base + resizeCall), 5);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(base + startupCall), 5);
    return true;
}

void OnSfseMessage(SFSE::MessagingInterface::Message* message)
{
    if (message && (message->type == SFSE::MessagingInterface::kPostLoad ||
                       message->type == SFSE::MessagingInterface::kPostPostLoad)) {
        PrepareUltrawideCreations();
    }
}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* sfse)
{
    if (!sfse || sfse->RuntimeVersion() != supportedRuntime) {
        return false;
    }
    SFSE::Init(sfse, { .trampoline = true, .trampolineSize = 128 });
    if (!InstallMarketplaceEntryHooks()) {
        return false;
    }
    const auto* messaging = SFSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(&OnSfseMessage)) {
        return false;
    }
    PrepareUltrawideCreations();
    return true;
}
