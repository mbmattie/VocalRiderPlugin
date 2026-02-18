# magic.RIDE: AAX Build & Pro Tools Distribution Guide

This guide walks you through building magic.RIDE as an AAX plugin for Pro Tools and preparing it for distribution.

---

## Part 1: Enabling AAX in the Build

JUCE bundles the AAX SDK, so **no separate SDK download is required** for building. You only need to add AAX to your plugin formats.

### Step 1: Update CMakeLists.txt

Add `AAX` to the FORMATS list in `CMakeLists.txt`:

```cmake
# Change this line:
FORMATS AU VST3 Standalone

# To:
FORMATS AU VST3 AAX Standalone
```

### Step 2: Build for AAX

**macOS:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Windows:** Build with Visual Studio (x64 configuration). The AAX build will appear in:
- `build/VocalRider_artefacts/Release/AAX/magic.RIDE.aaxplugin` (macOS)
- `build/VocalRider_artefacts/Release/AAX/magic.RIDE.aaxplugin` (Windows)

---

## Part 2: Testing AAX Locally (Development)

For **development and testing**, you can use Pro Tools without signing:

1. **Pro Tools Developer / Standard:** Pro Tools can load **unsigned** AAX plugins for testing. Copy your `.aaxplugin` to:
   - **macOS:** `/Library/Application Support/Avid/Audio/Plug-Ins/`
   - **Windows:** `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`

2. **iLok account:** You need an [iLok account](https://www.ilok.com) to run Pro Tools (for licensing the host, not necessarily for your plugin during development).

3. **Validate your build:** Avid provides an AAX Validator tool in their developer downloads. Use it to catch any descriptor or format issues before submission.

---

## Part 3: Commercial Distribution (Signing & Certification)

To **distribute** AAX plugins that load in standard (non-Developer) Pro Tools installations, your plugin must be **digitally signed** by Avid using PACE/iLok.

### Prerequisites

1. **Avid Developer Account**
   - Create an account at [avid.com](https://www.avid.com)
   - Go to [my.avid.com](https://my.avid.com) → "Linked Avid Accounts" → "Create Account" next to "Developer Account"

2. **iLok USB Key**
   - Required for AAX code signing
   - Purchase from [ilok.com](https://www.ilok.com) or authorized resellers
   - The signing process uses this hardware key

3. **Evaluation Toolkit**
   - Accept the click-through license at [Avid AAX SDK](https://developer.avid.com/aax)
   - Download the Evaluation Toolkit (includes documentation, validator, and signing tools)

### The Signing Process

1. **Contact Avid**  
   Email **audiosdk@avid.com** with:
   - Your intent to commercialize an AAX plugin
   - Plugin name and brief description
   - Company/publisher name

2. **Avid will provide:**
   - Access to the signing toolchain (PACE/iLok-based)
   - Instructions for building a signable AAX package
   - Any plugin ID or descriptor requirements

3. **Sign your plugin** using the provided tools and your iLok USB key.

4. **Optional: Avid Certification**  
   For listing in Avid’s marketplace or partner catalog:
   - Contact **partners@avid.com**
   - Certification involves testing and validation
   - Fees may apply; they’ll outline the process

---

## Part 4: Installer Integration (Future)

Once you have signed AAX builds, you can extend your installers:

- **macOS:** Add the `.aaxplugin` to your `build_installer.sh` staging and `pkgbuild`/`productbuild` steps, with install location `/Library/Application Support/Avid/Audio/Plug-Ins/`.
- **Windows:** Add the AAX plugin to your Inno Setup script (`magic.RIDE.iss`) and install to `{commonpf64}\Common Files\Avid\Audio\Plug-Ins\`.

---

## Quick Reference

| Stage | What You Need |
|-------|---------------|
| **Build & test locally** | CMake + JUCE (AAX in FORMATS), Pro Tools, iLok account |
| **Distribute commercially** | Avid dev account, iLok USB key, contact audiosdk@avid.com |
| **Avid Certified** | Contact partners@avid.com, certification fees |

**Useful links:**
- [Avid AAX Developer Page](https://developer.avid.com/aax)
- [iLok](https://www.ilok.com)
- Avid AAX support: **audiosdk@avid.com**
- Avid partners/certification: **partners@avid.com**
