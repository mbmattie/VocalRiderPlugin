# Local AAX Wrapping Guide

AAX wrapping (PACE signing) is done locally instead of on CI runners. GitHub Actions builds the unsigned AAX artifacts for both macOS and Windows. You then download them, wrap them on your local machines, and include them in your final installers.

---

## Overview

```
GitHub Actions (CI)          Local Machine
┌─────────────────┐         ┌──────────────────────────┐
│ Build macOS AAX  │───────▶│ Download unsigned macOS   │
│ Build Windows AAX│───────▶│ Download unsigned Windows │
│ (both unsigned)  │         │                          │
└─────────────────┘         │ Wrap macOS AAX (native)   │
                            │ Wrap Windows AAX (UTM VM) │
                            │                          │
                            │ Build final installers    │
                            └──────────────────────────┘
```

---

## Prerequisites

### On your Mac (native)

- **Eden SDK** installed (provides `wraptool` and `iloktool`)
  - Path: `/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool`
- **iLok account** with PACE signing credentials
- **Apple Developer ID** certificate in your Keychain

### In the Windows VM (UTM)

- **Eden SDK for Windows** installed (provides `wraptool.exe` and `iloktool.exe`)
  - Download from your private `mbmattie/build-tools` repo (release: `eden-sdk-v5.10.4`)
  - Run the installer — it will place tools under `C:\Program Files\PACE Anti-Piracy\Eden\`
- **iLok account** (same credentials as macOS)

---

## Step 1: Download Unsigned AAX Artifacts from GitHub

After a CI build completes, download the unsigned AAX artifacts.

### Using the GitHub CLI

```bash
# List recent workflow runs
gh run list --repo mbmattie/VocalRiderPlugin --limit 5

# Download unsigned AAX artifacts from a specific run
RUN_ID=<run-id-from-above>

gh run download $RUN_ID \
  --repo mbmattie/VocalRiderPlugin \
  --name "magic.RIDE-1.2.0-macOS-AAX-Unsigned" \
  --dir ~/Desktop/aax-unsigned/macos

gh run download $RUN_ID \
  --repo mbmattie/VocalRiderPlugin \
  --name "magic.RIDE-1.2.0-Windows-AAX-Unsigned" \
  --dir ~/Desktop/aax-unsigned/windows
```

### Using the GitHub Web UI

1. Go to **Actions** tab in your repo
2. Click the build run
3. Scroll to **Artifacts** at the bottom
4. Download `magic.RIDE-1.2.0-macOS-AAX-Unsigned` and `magic.RIDE-1.2.0-Windows-AAX-Unsigned`
5. Unzip both to `~/Desktop/aax-unsigned/macos/` and `~/Desktop/aax-unsigned/windows/`

---

## Step 2: Wrap macOS AAX (Native)

Run these commands from your Mac terminal:

```bash
WRAPTOOL="/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool"
AAX_IN="$HOME/Desktop/aax-unsigned/macos/magic.RIDE.aaxplugin"
AAX_OUT="$HOME/Desktop/aax-signed/macos/magic.RIDE.aaxplugin"

mkdir -p "$HOME/Desktop/aax-signed/macos"

# Open iLok cloud session
iloktool cloud --open --account "<ILOK_USERNAME>" --password "<ILOK_PASSWORD>" -v

# Wrap/sign the AAX plugin
$WRAPTOOL sign --verbose \
  --account "<ILOK_USERNAME>" \
  --signid "Developer ID Application: MATTHEW STEVEN ERNST (<TEAM_ID>)" \
  --customernumber "<PACE_CUSTOMER_NUMBER>" \
  --customername "MBM Audio" \
  --productname "magic.RIDE" \
  --allowsigningservice \
  --dsig1-compat off \
  --in "$AAX_IN" \
  --out "$AAX_OUT"

# Verify
$WRAPTOOL verify --verbose --in "$AAX_OUT"

# Close iLok session
iloktool cloud --close -v
```

---

## Step 3: Wrap Windows AAX (UTM VM)

### One-Time VM Setup

1. **Install Eden SDK** — Run the Eden SDK Windows installer inside the VM.
   The easiest way to get the installer into the VM is via a shared folder or by
   downloading it directly inside the VM from your private GitHub repo.

2. **Verify tools are available** — Open PowerShell in the VM and check:
   ```powershell
   # Typical install locations
   Get-ChildItem "C:\Program Files\PACE Anti-Piracy" -Recurse -Filter "wraptool.exe"
   Get-ChildItem "C:\Program Files\PACE Anti-Piracy" -Recurse -Filter "iloktool.exe"
   ```

3. **Set up a shared folder** between Mac and VM:
   - In UTM, select your VM → **Edit** → **Sharing**
   - Enable **Directory Sharing** and point it at a folder like `~/Desktop/aax-unsigned`
   - Inside Windows, the shared folder appears as a network drive or under
     `\\Mac\` — you may need to install SPICE WebDAV to access it

### Wrapping the Windows AAX

Open PowerShell in the VM:

```powershell
$WRAPTOOL = "C:\Program Files\PACE Anti-Piracy\Eden\Fusion\Versions\5\bin\wraptool.exe"
$ILOKTOOL = "C:\Program Files\PACE Anti-Piracy\Eden\Fusion\Versions\5\bin\iloktool.exe"
$AAX_IN   = "Z:\windows\magic.RIDE.aaxplugin"   # Shared folder path (adjust as needed)
$AAX_OUT  = "C:\Users\$env:USERNAME\Desktop\magic.RIDE_signed.aaxplugin"

# Open iLok cloud session
& $ILOKTOOL cloud --open --account "<ILOK_USERNAME>" --password "<ILOK_PASSWORD>" -v

# Wrap/sign the AAX plugin
& $WRAPTOOL sign --verbose `
  --account "<ILOK_USERNAME>" `
  --customernumber "<PACE_CUSTOMER_NUMBER>" `
  --customername "MBM Audio" `
  --productname "magic.RIDE" `
  --allowsigningservice `
  --dsig1-compat off `
  --in $AAX_IN `
  --out $AAX_OUT

# Verify
& $WRAPTOOL verify --verbose --in $AAX_OUT

# Close iLok session
& $ILOKTOOL cloud --close -v
```

Copy the signed `magic.RIDE_signed.aaxplugin` back to your Mac via the shared folder.

---

## Step 4: Build Final Installers

Once you have both signed AAX plugins, include them in your installers.

### macOS Installer

Copy the signed macOS AAX into the installer staging area and rebuild:

```bash
# Copy signed AAX to where the installer script expects it
cp -R ~/Desktop/aax-signed/macos/magic.RIDE.aaxplugin \
  build/VocalRider_artefacts/Release/AAX/magic.RIDE.aaxplugin

# Rebuild the installer
cd installer && ./build_installer.sh
```

Then sign, notarize, and staple the `.pkg` as usual.

### Windows Installer

Copy the signed Windows AAX into the build output and rebuild with Inno Setup:

```powershell
# In the VM, copy signed AAX to the expected build location
Copy-Item -Recurse -Force `
  "C:\Users\$env:USERNAME\Desktop\magic.RIDE_signed.aaxplugin" `
  "build\VocalRider_artefacts\Release\AAX\magic.RIDE.aaxplugin"

# Build installer with Inno Setup
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" installer\windows\magic.RIDE.iss
```

---

## Credential Reference

| Secret | Description |
|--------|-------------|
| `ILOK_USERNAME` | Your iLok account username |
| `ILOK_PASSWORD` | Your iLok account password |
| `PACE_CUSTOMER_NUMBER` | Provided by Avid/PACE for signing |
| `TEAM_ID` | Apple Developer Team ID (macOS only) |

---

## Troubleshooting

### wraptool "session not found" or auth errors
- Make sure `iloktool cloud --open` succeeded before running `wraptool sign`
- Check that your iLok credentials are correct
- Ensure your PACE customer number is active

### Windows VM shared folder not visible
- In UTM, check that Directory Sharing is enabled for the VM
- Inside Windows, install the **SPICE WebDAV** service if prompted by the guest tools
- Try accessing `\\localhost\DavWWWRoot\` in File Explorer

### wraptool not found in Windows VM
- The Eden SDK installer may place tools in a versioned path — search `C:\Program Files` recursively
- If you extracted with 7-Zip instead of running the installer, you may be missing runtime DLLs
