# TPX3 Emulator IOC Phoebus Display

## Overview

This directory contains Phoebus **Display Builder** (`.bob`) screens for the **multi-detector** emulator IOC (**Timepix3** / **Medipix3** today; more detectors later, e.g. Timepix4, via new **`emulator_*_embed.bob`** files). The **main** file is **`emulator.bob`**. That shell wires `EMULATOR_TYPE`, firmware/network, JVM/JAR/command port, the generated command line, and **two overlapping `embedded`** panels — **`emulator_tpx3_embed.bob`** when `EMULATOR_TYPE == 0`, **`emulator_mpx3_embed.bob`** when `== 1`. Keep embeds in this **same directory** so relative paths resolve. Targets Phoebus 4.7.4+.

## Files

- **`emulator.bob`** — Main shell (embedded displays + shared parameters)
- **`emulator_tpx3_embed.bob`** — Timepix3-only: hit rate, TDC, chipboard, JVM `-DchipMask`, raw replay strip
- **`emulator_mpx3_embed.bob`** — Medipix3 CLI options (`megapixel`, `chipMask`, `cornerText`, `loss`, `frac`, `omr0`)
- **`README.md`** - This file

## Display Layout

See the `.bob` files for exact widget placement. Logical sections:

### 1. Main Control Section (Top)
- **Start/Stop Slide Button**: Controls the TPX3 emulator process (connected to `$(P)$(R)START` PV)
- **Status LED**: Visual indicator showing if the process is running (green) or stopped (red) (connected to `$(P)$(R)STATUS` PV)
- **Process ID Display**: Shows the current process ID when running (connected to `$(P)$(R)PROCESS_ID` PV)
- **Error Message Display**: Shows any error messages or status updates (connected to `$(P)$(R)ERROR_MSG` PV)

### 2. Shell configuration ( **`emulator.bob`** )
- **`EMULATOR_TYPE`**: Choosing 0 vs 1 toggles visibility of the two embedded `.bob` files (rules use `$(P)$(R)EMULATOR_TYPE`; keep `$(P)` defined when launching the screen).
- **Firmware / network**: Shared widgets on the shell (same PVs as the IOC DB).
- **Embedded Timepix3 panel**: Hit rate, TDC, chipboard, JVM `-DchipMask`, and all raw replay widgets — see **`emulator_tpx3_embed.bob`**.
- **Embedded Medipix3 panel**: MPX CLI options — see **`emulator_mpx3_embed.bob`**.

### 3. JVM / JAR block (below embeds)
- **JAR**, **`-Xmx`**, **`-DcmdPort`**: Applicable to both emulator JAR types; replay controls are **not** here anymore (they are on the TPX embed).

### 4. Command line (bottom)
- **Generated Command Line**: Shows the complete Java command that will be executed (connected to `$(P)$(R)COMMAND_LINE` PV)
- **Real-time Updates**: Updates automatically as parameters are changed

## Supported Widget Types

The display uses the following Phoebus Display Builder compatible widgets:

- **`slide_button`** - For start/stop and enable/disable controls
- **`led`** - For status indicators
- **`spinner`** - For numeric parameter inputs
- **`textupdate`** - For displaying PV values as strings
- **`label`** - For static text and headers
- **`group`** - For layout containers

## Usage Instructions

### Opening the Display in Phoebus

1. **Launch Phoebus Display Builder**
2. **Open the BOB file**:
   - File → Open File
   - Navigate to `tpx3emulatorApp/op/bob/emulator.bob`
   - Select and open the file

### Configuring Parameters

1. **Set Parameter Values**:
   - Use the spinner controls to set numeric values
   - Use text input fields for file names and paths
   - All changes are applied immediately

2. **Enable/Disable Parameters**:
   - Use slide buttons for all enable/disable options
   - Only enabled parameters are included in the command line

3. **Start the Emulator**:
   - Use the Start/Stop slide button to control the process
   - Status LED shows green when running, red when stopped

### Monitoring

- **Status LED**: Visual indicator of process state
- **Process ID**: Shows the system process ID when running
- **Error Messages**: Displays any error messages or status updates as readable text
- **Command Line**: Shows the exact command being executed as readable text

## PV Naming Convention

The display uses the standard EPICS PV naming convention with predefined macros:
- **Prefix**: `$(P)` = `TPX3-TEST:`
- **Record**: `$(R)` = `Emulator:`
- **Example**: The START PV becomes `TPX3-TEST:Emulator:START`

## Display Features

### **Responsive Design**
- Main shell: about 1150×1360 pixels (embedded panels sized to TPX replay strip)
- Organized in logical groups with clear visual separation
- Color-coded sections for easy navigation

### **Real-time Updates**
- All PVs update in real-time
- Command line updates automatically as parameters change
- Status indicators provide immediate feedback
- String values displayed directly (no ASCII code interpretation)

### **User-Friendly Controls**
- Slide buttons for all enable/disable functionality
- Spinner controls for numeric values with appropriate ranges
- Text inputs for file names and paths
- Read-only displays for calculated values

### **Error Handling**
- Clear error message display as readable text
- Visual status indicators
- Process state monitoring

## Troubleshooting

### **Display Not Loading**
- Verify Phoebus version compatibility (4.7.4+ recommended)
- Check that the BOB file path is correct
- Ensure all widget types are supported

### **Widget Display Issues**
- **Common Issue**: Unknown widget type errors
- **Solution**: Use only supported widget types (slide_button, led, spinner, textupdate, label, group)
- **Prevention**: Always test displays in Phoebus Display Builder before deployment

### **PVs Not Connecting**
- Verify EPICS Channel Access is working
- Check that macros `$(P)` and `$(R)` are properly defined
- Ensure the IOC is running and accessible

### **Text Not Updating**
- Verify that PVs contain string values, not ASCII codes
- Check that `textupdate` widgets are properly configured
- Ensure PV connections are working

### **Parameter Changes Not Reflecting**
- Check that the IOC is running
- Verify PV connections in Phoebus
- Check for error messages in the error display

## Customization

The display can be customized by:
- **Modifying the BOB file** in Phoebus Display Builder
- **Adding new widgets** for additional functionality
- **Changing colors and layouts** to match your preferences
- **Adding alarm indicators** for critical parameters

## Support

For issues with the Phoebus display:
1. Check the error message display on the screen
2. Verify Phoebus and EPICS configuration
3. Check IOC logs for any errors
4. Ensure all required PVs are accessible
5. Validate widget types are supported by your Phoebus version

## Version Information

- **Display Version**: 2.0
- **Phoebus Compatibility**: Phoebus 4.7.4 and later
- **EPICS Version**: EPICS 7.x
- **Widget Types**: slide_button, led, spinner, textupdate, label, group
- **Last Updated**: August 2024
