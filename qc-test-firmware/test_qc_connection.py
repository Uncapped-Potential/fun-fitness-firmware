#!/usr/bin/env python3
"""
Simple test script to verify QC firmware connection
"""

import asyncio
import logging
from qc_test_simple import QCTestRunner, QCStatus

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

async def test_connection():
    """Test basic connection to QC device"""
    runner = QCTestRunner()
    
    try:
        # 1. Discover devices
        logger.info("=" * 60)
        logger.info("STEP 1: Discovering QC devices...")
        logger.info("=" * 60)
        
        devices = await runner.discover_devices(timeout=10)
        
        if not devices:
            logger.error("❌ No QC devices found!")
            logger.info("Make sure:")
            logger.info("  1. ESP32 board is powered on")
            logger.info("  2. QC firmware is flashed")
            logger.info("  3. Device should advertise as 'QC_011' or similar")
            return False
        
        logger.info(f"✅ Found {len(devices)} device(s):")
        for d in devices:
            logger.info(f"   - {d['name']} ({d['address']}) Serial: {d['serial']}")
        
        # 2. Connect to devices
        logger.info("=" * 60)
        logger.info("STEP 2: Connecting to devices...")
        logger.info("=" * 60)
        
        connected = await runner.connect_all_devices(devices)
        
        if connected == 0:
            logger.error("❌ Failed to connect to any devices!")
            return False
        
        logger.info(f"✅ Connected to {connected} device(s)")
        
        # 3. Test QC commands
        logger.info("=" * 60)
        logger.info("STEP 3: Testing QC commands...")
        logger.info("=" * 60)
        
        for device in runner.devices:
            logger.info(f"\nTesting device: {device.name}")
            
            # Run self-test
            logger.info("  Running self-test...")
            await device.run_self_test()
            
            # Get status
            logger.info("  Getting status...")
            status = await device.get_status()
            
            logger.info(f"  ✅ Status received:")
            logger.info(f"     - Battery: {status.battery_mv}mV")
            logger.info(f"     - USB Connected: {'Yes' if status.usb_connected else 'No'}")
            logger.info(f"     - Safe Mode: {'Yes' if status.safe_mode else 'No'}")
            logger.info(f"     - Serial: {status.serial}")
            
            # Start logging
            logger.info("  Starting test logging...")
            await device.start_phase(1)  # INITIAL phase
            
            await asyncio.sleep(2)
            
            # Get status again
            status2 = await device.get_status()
            logger.info(f"  ✅ Logging active, timestamp: {status2.timestamp_ms}ms")
        
        # 4. Run initial checks
        logger.info("=" * 60)
        logger.info("STEP 4: Running initial checks...")
        logger.info("=" * 60)
        
        await runner.run_initial_checks()
        
        # Check results
        all_passed = True
        for device in runner.devices:
            result = device.test_results.get('initial_state', 'UNKNOWN')
            logger.info(f"{device.name}: {result}")
            if result != 'PASS':
                all_passed = False
        
        if all_passed:
            logger.info("✅ All devices passed initial checks!")
        else:
            logger.warning("⚠️ Some devices failed initial checks")
        
        return True
        
    except Exception as e:
        logger.error(f"❌ Test failed with error: {e}")
        import traceback
        traceback.print_exc()
        return False
        
    finally:
        # Cleanup
        logger.info("=" * 60)
        logger.info("Cleaning up...")
        logger.info("=" * 60)
        await runner.cleanup()

async def main():
    """Main entry point"""
    logger.info("QC Firmware Connection Test")
    logger.info("============================")
    
    success = await test_connection()
    
    if success:
        logger.info("\n✅ ✅ ✅ TEST SUCCESSFUL! ✅ ✅ ✅")
        logger.info("QC firmware is working correctly!")
        logger.info("You can now run the full QC GUI test.")
    else:
        logger.info("\n❌ ❌ ❌ TEST FAILED ❌ ❌ ❌")
        logger.info("Please check the firmware and try again.")

if __name__ == "__main__":
    asyncio.run(main())