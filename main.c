/***************************************************************************//**
 * @file
 * @brief main() function.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Date:        08-07-2021
 * Author:      Dave Sluiter
 * Description: This code was created by the Silicon Labs application wizard
 *              and started as "Bluetooth - SoC Empty".
 *              It is to be used only for ECEN 5823 "IoT Embedded Firmware".
 *              The MSLA referenced above is in effect.
 *              
 *              This code was written with:
 *                  Gecko SDK 3.2.1
 *                  GNU ARM 10.2.1
 *
 ******************************************************************************/
 
 #include "main.h"
 
// Include logging for this file
#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"





int main(void)
{
  // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
  // Note that if the kernel is present, processing task(s) will be created by
  // this call.
  sl_system_init();

  // Initialize the application. For example, create periodic timer(s) or
  // task(s) if the kernel is present.
  // Students: Place all of your 1 time initialization code in this function.
  //           See app.c
  app_init();

#if defined(SL_CATALOG_KERNEL_PRESENT)

  // Start the kernel. Task(s) created in app_init() will start running.
  sl_system_kernel_start();
  
#else // SL_CATALOG_KERNEL_PRESENT

  // The so-called super-loop, sometimes called: "the main while(1) loop"
  while (1) {
  
    // Do not remove this call: Silicon Labs components process action routine
    // must be called from the super loop.
    // Students: This call will eventually call the function sl_bt_on_event()
    // defined in app.c which we will discuss in lecture and learn more about
    // in later programming assignments. Do not remove this call.
    sl_system_process_action();

    // Application process. See app.c
    app_process_action();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Let the CPU go to sleep if the system allows it.
    sl_power_manager_sleep();
#endif

  } // while
  
#endif // SL_CATALOG_KERNEL_PRESENT

} // main()
