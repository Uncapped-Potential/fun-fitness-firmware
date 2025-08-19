# Fun Fitness Controller Enclosure

3D printable case files for the Fun Fitness wireless game controller.

## Print Files

### Current Version Components (1 each)

1. **Up_FF_Body_V9_Rev_4.3mf** - Main controller body
2. **UP_FF_Lid_V9_Rev_4.3mf** - Top cover/lid

### Legacy Version (in Old/ folder)
- Up V7 Body - PC-FR Rev 2.obj
- Up V7 lid - PC-FRrev2.obj  
- Up V7 switch cradle - PC-FR Rev 3.obj

### Material Requirements

**⚠️ FIRE RETARDANT MATERIAL REQUIRED**

For any use beyond internal testing, all components **MUST** be printed in fire retardant (PC-FR) material:

- **Recommended Material**: Polycarbonate Fire Retardant (PC-FR)
- **Reason**: Contains 170mAh LiPo battery - fire safety is critical
- **Internal Testing**: Standard materials acceptable for prototyping only

### Print Settings

- **Layer Height**: 0.12mm
- **Infill**: 15%
- **Wall Loops**: 2
- **Supports**: Minimal - **CRITICAL**: Only add supports to outer perimeter of holes
  - Do NOT add supports inside tight holes (impossible to remove)
  - See photos in `print orientation photos and settings/` folder for detailed guidance
- **Print Orientation**: Optimal orientation shown in photos
- **Note**: Bambu print profile available (may need adjustment for Prusa A1)

For detailed print settings and support placement, refer to the photos in the `print orientation photos and settings/` directory. Contact Cooper if you have any questions or problems.

### Assembly Instructions

1. **Prepare the Enclosure**
   - Press-fit two M1.2 x 0.25mm nuts into designated holes in the body
   - Ensure nuts are seated properly for threading

2. **Install CodeCell**
   - Insert CodeCell into the body
   - Secure with 4x M1.2 self-tapping screws (included with CodeCell)

3. **Assemble Switch Harness**
   - Connect power switch inline with battery using JST connectors
   - (Additional switch harness specifications to be added)

4. **Install Battery**
   - Cut small piece of adhesive velcro
   - Apply velcro to designated area in enclosure (see photos)
   - Apply matching velcro to battery
   - Press battery firmly into place
   - Ensure secure attachment with reasonable amount of velcro

5. **Cable Management**
   - Route JST connectors and cables through cable routing clips
   - Ensure all connections are secure
   - Verify nothing interferes with lid closure

6. **Final Assembly**
   - Perform shake test - everything should be secure
   - Align lid with body
   - Insert M1.2 x 0.25mm screws (4-6mm length) through lid into press-fit nuts
   - Tighten screws carefully - do not overtighten

### Required Hardware

- **2x M1.2 x 0.25mm screws** (4-6mm length) - [Amazon B0D8PMBPB7](https://www.amazon.com/dp/B0D8PMBPB7)
- **2x M1.2 x 0.25mm nuts** - [Amazon B0DBHNJLTD](https://www.amazon.com/dp/B0DBHNJLTD)
- **4x M1.2 self-tapping screws** (included with CodeCell)
- **Adhesive velcro** for battery mounting

### Safety Notes

- **Battery Inspection**: CRITICAL - Thoroughly inspect battery for any damage before installation
- **Lid Closure**: Ensure nothing is pressing on the battery when closing lid (risk of damage)
- **Velcro Attachment**: Battery must be properly secured with adequate velcro to prevent movement
- **Fire Safety**: Use fire retardant materials for production builds
- **Storage**: Store and charge in LiPo safety bag (see main README)
- **Ventilation**: Ensure proper ventilation around device during charging
- **Inspection**: Check enclosure for cracks or damage before each use
- **Quality Control**: Additional QC specifications forthcoming
- **Handling**: Use proper ESD precautions when handling electronics

## File Format

Files are provided in 3MF format with embedded print settings. The 3MF format is compatible with most modern 3D printing software including:

- Bambu Studio (native support)
- PrusaSlicer 
- Cura
- Other slicers supporting 3MF import

Legacy OBJ files with MTL material definitions are available in the Old/ folder for compatibility with older software.

## Support Resources

- **Print Settings Photos**: See `print orientation photos and settings/` folder
- **Questions/Problems**: Contact Cooper for assistance
- **Main Documentation**: See parent directory README for complete BOM and specifications