# AAX Wrapping Guide

AAX wrapping (PACE signing) is handled differently per platform:

- **macOS AAX** — wrapped on CI *or* locally on your Mac (both use native `wraptool`)
- **Windows AAX** — wrapped on CI only (the x86_64 PACE tools don't run on ARM VMs)

When PACE credentials are configured as GitHub Secrets, the CI workflow wraps both platforms automatically. The installers include the wrapped AAX, and the individual AAX artifacts are also uploaded.

---

## CI Wrapping (Automatic)

The `build.yml` workflow handles AAX wrapping when PACE secrets are present. If the secrets aren't configured, the build still succeeds — it just produces unsigned AAX plugins.

### Required GitHub Secrets

Add these in **Settings → Secrets and variables → Actions** on your repo:

| Secret | Description |
|--------|-------------|
| `ILOK_USERNAME` | Your iLok account username |
| `ILOK_PASSWORD` | Your iLok account password |
| `PACE_CUSTOMER_NUMBER` | Provided by Avid/PACE for your signing agreement |
| `BUILD_TOOLS_TOKEN` | GitHub PAT with access to `mbmattie/build-tools` (already configured) |
| `WIN_CERT_PFX_BASE64` | Base64-encoded Windows OV code signing certificate (.pfx) |
| `WIN_CERT_PFX_PASSWORD` | Password for the Windows PFX certificate |

The following secrets should already be configured from the Apple signing setup:

| Secret | Already Configured |
|--------|-------------------|
| `APPLE_TEAM_ID` | Used for macOS `wraptool --signid` |
| `MACOS_APP_CERTIFICATE` | Developer ID Application cert (keychain) |
| `MACOS_INSTALLER_CERTIFICATE` | Developer ID Installer cert |
| `MACOS_CERTIFICATE_PWD` | P12 password for certs |

### How It Works

**macOS runner:**
1. Downloads Eden SDK DMG from `mbmattie/build-tools` (release `eden-sdk-mac-v5.10.4`)
2. Installs the SDK (provides `wraptool` and `iloktool`)
3. Sets the CI keychain as default (PACE docs workaround for runners)
4. Opens iLok Cloud session → wraps AAX → verifies → closes session
5. Replaces the unsigned AAX in the build directory before building the installer

**Windows runner:**
1. Downloads `EdenSDKLiteInstallerWin64.zip` from `mbmattie/build-tools` (release `eden-sdk-v5.10.4`)
2. Installs the full Eden SDK (provides `wraptool`, `iloktool`, and PACE runtime)
3. Decodes the OV certificate PFX from `WIN_CERT_PFX_BASE64` secret
4. Opens iLok Cloud session → wraps AAX with `--keyfile` → verifies → closes session
5. Replaces the unsigned AAX in the build directory before building the installer

**Note:** Windows AAX wrapping requires a real CA-issued OV certificate. wraptool uses Windows CryptoAPI internally for Authenticode signing — self-signed certificates cause it to hang during CRL/OCSP revocation checking. If `WIN_CERT_PFX_BASE64` is not set, the step is skipped gracefully.

### Build-Tools Release Assets

| Release | Asset | Purpose |
|---------|-------|---------|
| `eden-sdk-mac-v5.10.4` | `EdenSDKLiteInstallerMac_v5.10.4_72ed0501.2.dmg` | macOS Eden SDK installer |
| `eden-sdk-v5.10.4` | `EdenSDKLiteInstallerWin64.zip` | Windows Eden SDK installer |

---

## Local macOS Wrapping (Optional)

If you prefer to wrap the macOS AAX locally instead of on CI, or need to re-wrap an artifact:

### Prerequisites

- **Eden SDK** installed on your Mac (provides `wraptool` and `iloktool`)
  - Path: `/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool`
- **iLok account** with PACE signing credentials
- **Apple Developer ID** certificate in your Keychain

### Download Unsigned AAX from CI

```bash
# List recent workflow runs
gh run list --repo mbmattie/VocalRiderPlugin --limit 5

# Download the macOS AAX artifact
RUN_ID=<run-id-from-above>
gh run download $RUN_ID \
  --repo mbmattie/VocalRiderPlugin \
  --name "magic.RIDE-1.2.0-macOS-AAX" \
  --dir ~/Desktop/aax-unsigned/macos
```

Or download from the GitHub web UI: **Actions → click run → Artifacts → download**.

### Wrap the AAX

```bash
WRAPTOOL="/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool"
AAX_IN="$HOME/Desktop/aax-unsigned/macos/magic.RIDE.aaxplugin"
AAX_OUT="$HOME/Desktop/aax-signed/macos/magic.RIDE.aaxplugin"

mkdir -p "$HOME/Desktop/aax-signed/macos"

iloktool cloud --open --account "<ILOK_USERNAME>" --password "<ILOK_PASSWORD>" -v

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

$WRAPTOOL verify --verbose --in "$AAX_OUT"

iloktool cloud --close -v
```

### Rebuild Installer with Signed AAX

```bash
cp -R ~/Desktop/aax-signed/macos/magic.RIDE.aaxplugin \
  build/VocalRider_artefacts/Release/AAX/magic.RIDE.aaxplugin

cd installer && ./build_installer.sh
```

Then sign, notarize, and staple the `.pkg` as usual.

---

## Troubleshooting

### wraptool "session not found" or auth errors
- Make sure `iloktool cloud --open` succeeded before running `wraptool sign`
- Check that your iLok credentials are correct
- Ensure your PACE customer number is active
- iLok Cloud can only be active on one machine at a time — close other sessions first

### CI wrapping step skipped
- The wrapping steps only run when `ILOK_USERNAME` is set as a GitHub Secret
- Check **Settings → Secrets** on your repo

### macOS CI keychain errors
- The workflow sets `security default-keychain` before running wraptool (PACE docs workaround)
- If wraptool still fails with keychain errors, check that `MACOS_APP_CERTIFICATE` is valid

### Windows wraptool hangs on CI
- wraptool uses Windows CryptoAPI directly for Authenticode signing (not `signtool.exe`)
- Self-signed certificates have no CRL/OCSP endpoint, causing CryptoAPI to hang
- **Solution:** Use a real CA-issued OV certificate (has proper CRL endpoints)
- Add `WIN_CERT_PFX_BASE64` and `WIN_CERT_PFX_PASSWORD` secrets with your OV cert
- To encode your PFX: `base64 -i your-cert.pfx | pbcopy` (macOS) or `certutil -encode your-cert.pfx encoded.txt` (Windows)
