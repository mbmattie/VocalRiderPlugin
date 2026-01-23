# magic.RIDE - Complete Feature Documentation

## Overview
magic.RIDE is a precision vocal leveling plugin by MBM Audio. It automatically rides the gain of vocal recordings to maintain consistent levels, similar to how a live sound engineer would ride a fader during a performance.

---

## Main Controls (Center Panel)

### TARGET Knob (Large Center Knob)
- **Range**: -40 dB to -6 dB
- **Default**: -18 dB
- **What it does**: Sets the target level where you want your vocal to sit consistently
- **Technical**: Uses RMS/LUFS detection to measure average vocal level and adjusts gain to maintain this target
- **Recommended use**:
  - **Lead Vocals (Pop/R&B)**: -18 dB to -16 dB (prominent in mix)
  - **Background Vocals**: -24 dB to -20 dB (sits behind lead)
  - **Podcast/Voiceover**: -16 dB to -12 dB (voice is primary focus)
  - **Broadcast**: -23 LUFS (EBU R128 standard)
- **Tips**: 
  - Use AUTO-TARGET button to analyze your vocal and automatically set this
  - Lower values = quieter vocal in final mix
  - Higher values = louder, more prominent vocal

### RANGE Knob (Left Side)
- **Range**: 3 dB to 24 dB
- **Default**: 12 dB
- **What it does**: Sets the maximum amount of gain boost or cut the plugin can apply
- **Technical**: Split equally between boost and cut (e.g., 12 dB range = ±6 dB of gain adjustment)
- **Recommended use**:
  - **Gentle Leveling**: 6-9 dB (subtle, natural)
  - **Standard Vocals**: 12-15 dB (typical commercial music)
  - **Aggressive/Dynamic Performance**: 18-24 dB (podcast with varying distance from mic)
- **Tips**:
  - Too much range (>18 dB) can sound unnatural or "pumpy"
  - Too little range (<6 dB) may not catch all dynamics
  - Range lines visible in waveform display (dashed gray)

### SPEED Knob (Right Side)
- **Range**: 0% to 100%
- **Default**: 50%
- **What it does**: Controls how quickly the plugin responds to level changes
- **Technical**: Automatically adjusts attack and release times (visible in Advanced panel)
  - 0% (Slow): Attack ~500ms, Release ~1000ms - smooth, slow movements
  - 50% (Medium): Attack ~200ms, Release ~500ms - balanced
  - 100% (Fast): Attack ~5ms, Release ~20ms - very responsive
- **Recommended use**:
  - **Slow (0-30%)**: Ballads, sustained singing, speech with consistent delivery
  - **Medium (30-70%)**: Most pop/rock vocals, general purpose
  - **Fast (70-100%)**: Rap, very dynamic singing, conversational podcasts
- **Tips**:
  - Faster speeds catch quick level changes but can sound more "worked"
  - Slower speeds sound more natural but may miss fast transients
  - Use NATURAL mode with fast speeds to maintain musical feel

### GAIN Meter (Small Bar Meter)
- **Display**: Center = 0 dB, Up = boost (cyan), Down = cut (purple)
- **Range**: ±12 dB display
- **What it does**: Shows real-time gain changes being applied by the plugin
- **Technical**: Instant visual feedback of gain adjustment, matches colors in waveform display
- **Recommended use**: Monitor this while adjusting RANGE and SPEED to see how hard the plugin is working
- **Tips**:
  - If constantly at max/min, increase RANGE or adjust TARGET
  - Minimal movement = vocal already well-balanced or RANGE too small

---

## Bottom Bar Controls

### NATURAL Toggle
- **Default**: OFF
- **What it does**: Preserves the natural dynamics and expression of the performance
- **Technical**: Reduces gain adjustment during intentional dynamics (crescendos, emphasized words) by analyzing rate of level change
- **Recommended use**:
  - Turn ON for expressive, dynamic singing where the performer intentionally varies volume
  - Turn OFF for consistent speech, podcast, or when you want maximum leveling
- **Tips**: 
  - Works great with faster SPEED settings to prevent over-compression feel
  - Ideal for preserving artistic intent in ballads or emotional performances

### SMART SILENCE Toggle
- **Default**: OFF
- **What it does**: Automatically reduces gain during silence or very quiet passages
- **Technical**: Detects when input falls below threshold and reduces gain to prevent amplifying noise floor
- **Recommended use**:
  - Turn ON for recordings with breath noise, room ambience, or noisy mic preamps
  - Turn OFF for clean studio recordings or when silence is already clean
- **Tips**:
  - Helps prevent "pumping up" of background noise between phrases
  - Works with BREATH REDUCTION in Advanced panel for comprehensive breath control

### AUTO-TARGET Button
- **Function**: Click to activate 3-second learning mode (button pulses while learning)
- **What it does**: Analyzes your vocal for 3 seconds and automatically sets TARGET, RANGE, and SPEED
- **Technical**: Measures min, max, and average levels; calculates optimal TARGET (average level) and RANGE (60% of dynamic range)
- **Recommended use**:
  - Use at the start of your session to get a good starting point
  - Re-run if the vocal changes character (verse vs. chorus)
- **Tips**:
  - Play a representative section of the vocal (not just quiet parts)
  - Include both quiet and loud moments for accurate analysis
  - Results are immediately applied but can be manually adjusted after

### SPEED Button (Waveform Display Speed)
- **Options**: 10s (Fast), 15s (Medium), 20s (Slow)
- **Default**: 15s
- **What it does**: Controls how fast the waveform scrolls across the screen
- **Recommended use**:
  - 10s: Quick overview, fast playback
  - 15s: General purpose, good detail
  - 20s: Detailed analysis, slow songs
- **Tips**: Purely visual preference, doesn't affect audio processing

### Automation Mode Selector (Right Side)
- **Options**: READ, WRITE, TOUCH, LATCH
- **Default**: READ
- **What it does**: Controls how the plugin interacts with DAW automation
- **Technical**:
  - **READ**: Plugin processes audio but doesn't write automation
  - **WRITE**: Continuously writes gain adjustments to DAW automation
  - **TOUCH**: Writes automation only while adjusting a parameter
  - **LATCH**: Starts writing when you touch a control, continues until playback stops
- **Recommended use**:
  - Use WRITE to record the plugin's automatic gain adjustments as automation
  - Use TOUCH/LATCH for hands-on fader riding with automation capture
  - Dropdown pulses with purple glow when actively writing automation
- **Tips**: Check your DAW's automation lane to see recorded gain changes

---

## Advanced Panel (Click Gear Icon)

### LOOK-AHEAD Dropdown
- **Options**: Off, 5ms, 10ms, 20ms
- **Default**: Off
- **What it does**: Enables the plugin to "see ahead" in time and prepare gain changes before loud/quiet sections arrive
- **Technical**: Introduces matching latency; DAW compensates automatically
- **Recommended use**:
  - **Off**: Minimal latency, real-time monitoring
  - **5-10ms**: Smoother gain transitions, imperceptible latency
  - **20ms**: Most natural leveling, suitable for mixing (not live use)
- **Tips**: Higher look-ahead = smoother automation curve, slightly delayed response

### DETECTION Dropdown
- **Options**: RMS, LUFS
- **Default**: RMS
- **What it does**: Chooses the method for measuring vocal level
- **Technical**:
  - **RMS**: Standard average power measurement, faster response
  - **LUFS**: Perceptually weighted loudness standard (EBU R128), slower but more accurate to human hearing
- **Recommended use**:
  - **RMS**: Music production, general vocals
  - **LUFS**: Broadcast, podcast, streaming content (adheres to loudness standards)

### ATTACK Knob
- **Range**: 1ms to 500ms
- **Auto-set by SPEED knob**
- **What it does**: How quickly gain increases when the vocal gets quieter
- **Recommended use**:
  - **Fast (1-20ms)**: Rap, speech, catches every syllable
  - **Medium (50-200ms)**: Pop/rock vocals
  - **Slow (300-500ms)**: Smooth, ballad-style leveling
- **Tips**: Faster attack can sound more aggressive; slower is more musical

### RELEASE Knob
- **Range**: 10ms to 2000ms
- **Auto-set by SPEED knob**
- **What it does**: How quickly gain decreases when the vocal gets louder
- **Recommended use**:
  - **Fast (10-50ms)**: Aggressive leveling, speech
  - **Medium (100-500ms)**: Standard vocals
  - **Slow (1000-2000ms)**: Gentle, program-style leveling
- **Tips**: Slower release prevents pumping on sustained notes

### HOLD Knob
- **Range**: 0ms to 500ms
- **Default**: 50ms
- **What it does**: Maintains current gain level for this duration before releasing
- **Technical**: Prevents rapid gain changes during brief pauses or consonants
- **Recommended use**:
  - **Short (0-50ms)**: Responsive, fast leveling
  - **Medium (50-150ms)**: Balanced, prevents fluttering
  - **Long (200-500ms)**: Smooth program-style leveling, prevents gain pumping
- **Tips**: Increase if you hear rapid gain fluctuations between words

### BREATH Knob
- **Range**: 0 dB to 12 dB
- **Default**: 0 dB (off)
- **What it does**: Reduces the level of breath sounds and quiet passages
- **Technical**: Detects breaths by analyzing frequency content and level, applies selective gain reduction
- **Recommended use**:
  - **Light (3-6 dB)**: Subtle breath control, still natural
  - **Medium (6-9 dB)**: Clear breath reduction without artifacts
  - **Heavy (9-12 dB)**: Aggressive breath removal, may sound processed
- **Tips**: 
  - Combine with SMART SILENCE for comprehensive breath control
  - Don't over-do it or vocal will sound unnatural
  - Best used after recording with proper mic technique

### TRANSIENT Knob
- **Range**: 0% to 100%
- **Default**: 0% (off)
- **What it does**: Preserves transient peaks (consonants like 't', 'k', 's') even during gain reduction
- **Technical**: Fast envelope follower detects transients and temporarily reduces gain reduction
- **Recommended use**:
  - **0%**: Maximum leveling, smoothest result
  - **50%**: Balanced - maintains some transient energy
  - **100%**: Preserves all transient character, less leveling on consonants
- **Tips**:
  - Use for rap vocals to maintain punchiness
  - Lower settings for smooth, polished vocals
  - Higher settings preserve "presence" and clarity

### OUTPUT Meter (Vertical Fader)
- **Range**: -12 dB to +12 dB
- **Default**: 0 dB
- **What it does**: Output trim (makeup gain) applied after all processing
- **Technical**: Clean gain stage after leveling; allows matching input/output levels
- **Recommended use**:
  - Adjust to match bypassed level (unity gain)
  - Add makeup gain if TARGET is set very low
  - Reduce if clipping occurs in your DAW
- **Tips**:
  - Watch the OUTPUT meter (O) on right side - keep it out of red
  - This gain is applied last, doesn't affect how the plugin analyzes the vocal

---

## Header Controls

### Preset Dropdown
- **Function**: Save and recall complete plugin states
- **What it includes**: All parameters, advanced settings, and toggle states
- **Categories**:
  - **Init**: Reset to factory defaults (-18 dB target, 50% speed, 12 dB range)
  - **Vocals**: Presets for singing (Gentle Vocal, Aggressive Leveling, etc.)
  - **Speaking**: Presets for speech/podcast (Broadcast Voice, etc.)
  - **Mattie's Favorites**: Curated starting points
- **Tips**: 
  - Use arrows (◄ ►) to navigate presets quickly
  - Preset selection is saved with your DAW project
  - Start with a preset, then fine-tune to taste

### A/B Compare Button
- **Function**: Toggle between two complete plugin states for comparison
- **What it does**: Stores current settings as "A" or "B" state; switch instantly to compare
- **Recommended use**:
  - Try different TARGET settings and compare
  - Test aggressive vs. gentle RANGE amounts
  - A/B your adjustments against the preset starting point
- **Tips**: Great for making informed decisions - trust your ears, not just the meters

### Undo/Redo Buttons (← →)
- **Function**: Step backward/forward through parameter changes
- **Keyboard shortcut**: Cmd+Z (Undo), Cmd+Shift+Z (Redo)
- **What it tracks**: Any knob adjustment, preset load, or parameter change
- **Tips**: Don't be afraid to experiment - you can always undo

### Gear Icon (Advanced Settings)
- **Function**: Opens/closes the Advanced panel
- **Visual indicator**: Glows purple when panel is open
- **What it reveals**: Attack/Release/Hold timing, breath reduction, transient preservation, output trim

### Question Mark Icon (About/Help)
- **Function**: Click to open About dialog
- **What it shows**: Version info, credits, website link, hover help instructions
- **Hover Help**: Hover over any control for 3 seconds to see detailed help text automatically

---

## Waveform Display (Main Visual)

### Real-Time Waveform
- **White/Gray waveform**: Shows INPUT signal (original audio before processing)
- **Scrolls right-to-left**: New audio appears on right, history scrolls left
- **Logarithmic scale**: Mimics human hearing, shows detail in quiet and loud sections

### Target Line
- **Color**: Light purple gradient (matches logo)
- **Position**: Your TARGET setting visualized
- **Interactive**: Click and drag the line to adjust TARGET in real-time
- **Label**: Shows current TARGET value in dB

### Range Lines
- **Color**: Muted gray, dashed lines
- **Position**: Above and below TARGET line
- **What they show**: Maximum boost/cut boundaries based on RANGE setting
- **Interactive**: Drag to adjust RANGE parameter
- **Labels**: Show ±dB values

### Gain Curve Line
- **Colors**: 
  - **Electric cyan/blue**: Gain boost (plugin making vocal louder)
  - **Bright purple**: Gain cut (plugin making vocal quieter)
- **What it shows**: The actual gain adjustment curve being applied in real-time
- **Visual feedback**: This is your "key selling feature" - see exactly what the plugin is doing
- **Tips**: 
  - Flat line = no adjustment needed (vocal already at target)
  - Smooth curves = natural-sounding processing
  - Jagged/extreme curves = consider adjusting SPEED or RANGE

### Cut Waveform Overlay
- **Color**: Transparent purple filled shape
- **What it shows**: The OUTPUT waveform (after gain reduction) when cutting
- **Technical**: Only visible when gain reduction is applied (>0.3 dB cut)
- **Use**: See the "before and after" - white waveform is original, purple overlay is result

### Background Colors (Subtle Tints)
- **Green tint**: Sections where gain is being boosted
- **Red tint**: Sections where gain is being cut
- **Purpose**: Quick visual indication of plugin activity without detailed analysis

---

## Meters (Right Side)

### OUTPUT Meter (O)
- **Display**: Vertical bar, green to yellow to red gradient
- **Range**: -48 dB to 0 dB
- **What it shows**: Final output level after all processing
- **Peak hold**: White line shows recent peak
- **Clip indicator**: Red at top if approaching 0 dB
- **Technical**: Uses moderate smoothing for stable, readable display
- **Tips**: Keep this out of red zone to prevent clipping in your DAW

### GAIN Meter (G)
- **Display**: Bidirectional bar (center = 0 dB)
- **Colors**: Cyan gradient (boost) to purple gradient (cut)
- **Range**: ±12 dB
- **What it shows**: Same as gain curve line but as a traditional meter
- **Purpose**: Quick glance at how much gain change is happening

---

## Workflow Examples

### Quick Start Workflow
1. Load magic.RIDE on your vocal track
2. Play a representative section of your vocal (verse + chorus)
3. Click AUTO-TARGET button and let it analyze for 3 seconds
4. Fine-tune TARGET if needed (±2-3 dB typically)
5. Adjust SPEED based on song tempo and vocal style
6. Done! Monitor the gain curve in waveform display

### Podcast Workflow
1. Load "Broadcast Voice" preset
2. Turn ON SMART SILENCE
3. Increase BREATH to 6-9 dB
4. Set DETECTION to LUFS
5. Adjust TARGET to -16 dB for prominent voice
6. Set SPEED to 60-80% for natural speech following

### Dynamic Vocal Workflow
1. Start with "Gentle Vocal" preset
2. Turn ON NATURAL to preserve dynamics
3. Increase RANGE to 15-18 dB
4. Adjust SPEED to medium (40-60%)
5. Fine-tune TRANSIENT to 30-50% to keep consonants punchy
6. Use A/B compare to check natural vs. processed feel

### Automation Recording Workflow
1. Set automation mode to WRITE or LATCH
2. Play through song while plugin adjusts levels
3. Watch automation dropdown pulse (purple glow = writing)
4. Set mode back to READ when done
5. Edit recorded automation curves in DAW if needed

---

## Technical Specifications

### Processing
- **Algorithm**: RMS/LUFS level detection with smooth gain ramping
- **Latency**: 0-20ms depending on LOOK-AHEAD setting (automatically compensated by DAW)
- **Soft Ceiling**: Built-in soft limiter at -0.3 dB prevents clipping
- **Gain Range**: Maximum ±12 dB (adjustable via RANGE control)
- **Sample Rate Support**: 44.1 kHz to 192 kHz
- **Bit Depth**: 32-bit float internal processing

### Formats
- **VST3**: Windows and macOS
- **AU**: macOS (Logic Pro, GarageBand)
- **Standalone**: Test without DAW

### System Requirements
- macOS 10.13 or later
- Compatible with all major DAWs (Logic, Pro Tools, Ableton, Reaper, etc.)

---

## Tooltips & Help

### Quick Value Tooltips
- **Hover over any knob**: Instant tooltip shows current value
- **While dragging**: Tooltip follows your mouse showing real-time value
- **Fade behavior**: Smooth fade-in/out for professional feel

### Extended Help Tooltips
- **How to access**: Hover over any control for 3 seconds
- **What they show**: Detailed explanation of the control and recommended settings
- **Automatic**: Tooltip automatically upgrades from value display to help text after 3 seconds
- **Available for**: All knobs, meters, and buttons (main panel, advanced panel, and bottom bar)

---

## Advanced Features (Power Users)

### Keyboard Shortcuts
- **Cmd+Z** (Mac) / **Ctrl+Z** (PC): Undo last parameter change
- **Cmd+Shift+Z** / **Ctrl+Shift+Z**: Redo
- **Cmd+Y** / **Ctrl+Y**: Redo (alternative)

### Window Scaling
- Click resize button (bottom-right corner)
- Options: Small, Medium, Large sizes
- Continuous scaling with scroll wheel
- All UI elements scale proportionally
- **Purpose**: Adapt to different screen sizes and personal preference

### State Persistence
- All settings saved with DAW project
- Includes: Main controls, advanced settings, toggles, preset selection, window size, automation mode
- Reopen plugin window anytime - settings preserved

### Clickable Logo
- Click "MBM AUDIO" (left section of logo) to visit musicbymattie.com
- Opens in default browser
- "magic.RIDE" text is not clickable (visual branding only)

---

## Tips for Best Results

### Recording
- Record vocals at proper gain (averaging around -18 dB before processing)
- Use good mic technique - consistent distance from mic
- Clean recordings give best results - garbage in = garbage out

### Mixing
- Place magic.RIDE early in your chain (before heavy compression/EQ)
- Use it to handle macro dynamics (phrase-to-phrase variation)
- Follow with compressor for micro dynamics (syllable-to-syllable consistency)
- The two tools work together: magic.RIDE rides the level, compressor controls peaks

### Avoiding Common Mistakes
- **Don't over-range**: >18 dB can sound unnatural
- **Don't set TARGET too high**: Risk clipping and overly loud vocal
- **Don't ignore the waveform**: Visual feedback is your friend
- **Don't skip AUTO-TARGET**: It's faster than guessing optimal settings
- **Don't forget to check bypassed sound**: Make sure processing is improvement, not just different

### Automation Best Practices
- Use WRITE mode to capture plugin's automatic adjustments
- Edit automation curves afterward if needed for manual touch-ups
- Consider printing/bouncing automated vocal to free up CPU
- Save automation mode in project so you don't accidentally overwrite

---

## Version Information
- **Current Version**: Displayed in bottom-left corner (e.g., "v1.0.0")
- **Updates**: Check musicbymattie.com for latest version
- **Support**: Contact through website for issues or feature requests

---

## Credits
- **Developer**: MBM Audio
- **Website**: musicbymattie.com
- **Plugin Name**: magic.RIDE
- **Tagline**: Precision Vocal Leveling

---

This documentation covers all features as of version 1.0.0. For video tutorials and audio examples, visit musicbymattie.com/magic-ride
