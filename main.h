/***********************************************************************
 * @file      main.h
 * @version   1.0
 * @brief     Main header file.
 *
 * @author    Dave Sluiter, David.Sluiter@colorado.edu
 * @date      March 17, 2021
 *
 * @institution University of Colorado Boulder (UCB)
 * @course      ECEN 5823: IoT Embedded Firmware
 * @instructor  David Sluiter
 *
 * @assignment Starter code
 * @due        NA
 *
 * @resources  Update this as appropriate.
 *
 * @copyright  All rights reserved. Distribution allowed only for the
 * use of assignment grading. Use of code excerpts allowed at the
 * discretion of author. Contact for permission.
 */

#ifndef MAIN_H
#define MAIN_H

#include "sl_component_catalog.h"
#include "sl_system_init.h"

#include "app.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif // SL_CATALOG_POWER_MANAGER_PRESENT
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_system_kernel.h"
#else // SL_CATALOG_KERNEL_PRESENT
#include "sl_system_process_action.h"
#endif // SL_CATALOG_KERNEL_PRESENT







#endif

