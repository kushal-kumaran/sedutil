/* C:B**************************************************************************
   This software is Copyright (c) 2014-2024 Bright Plaza Inc. <drivetrust@drivetrust.com>

   This file is part of sedutil.

   sedutil is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   sedutil is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with sedutil.  If not, see <http://www.gnu.org/licenses/>.

   * C:E********************************************************************** */


#include "DtaDevOSDrive.h"
#include "DtaDevLinuxDrive.h"
#include "DtaDevLinuxNvme.h"
#include "DtaDevLinuxScsi.h"


/** Factory functions
 *
 * Static class members that support instantiation of subclass members
 * with the subclass switching logic localized here for easier maintenance.
 *
 */



/* C:B**************************************************************************
   This software is Copyright (c) 2014-2024 Bright Plaza Inc. <drivetrust@drivetrust.com>

   This file is part of sedutil.

   sedutil is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   sedutil is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with sedutil.  If not, see <http://www.gnu.org/licenses/>.

   * C:E********************************************************************** */
#include "os.h"
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <scsi/sg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/hdreg.h>
#include <errno.h>
#include <vector>
#include <fstream>
#include <map>
#include <fnmatch.h>
#include <linux/fs.h>
#include <systemd/sd-device.h>

#include "DtaStructures.h"
#include "DtaDevLinuxSata.h"
#include "DtaDevLinuxScsi.h"
#include "DtaHexDump.h"



static
dictionary * getOSSpecificInformation(OSDEVICEHANDLE osDeviceHandle,
                                      const char * devref,
                                      InterfaceDeviceID & interfaceDeviceIdentification,
                                      DTA_DEVICE_INFO &device_info) {

  device_info.devType = DEVICE_TYPE_OTHER;

  int r;

  // Special `ioctl` to get the device size
  device_info.devSize = 0;
  r = ioctl(handleDescriptor(osDeviceHandle), BLKGETSIZE64, &device_info.devSize);
  if (r < 0) {
    errno = -r;
    fprintf(stderr, "Failed to get device size: %m for device %s osDeviceHandle 0x%16p\n", devref, osDeviceHandle);
  }

  // Get the `sd_device` to extract properties
  __attribute__((cleanup(sd_device_unrefp))) sd_device *device = NULL;
  r = sd_device_new_from_devname(&device, devref);
  if (r < 0) {
    errno = -r;
    fprintf(stderr, "Failed to allocate sd_device: %m\n");
    return NULL;
  }

  // Get device properties from `sd_device device`
  dictionary * pDeviceProperties = new dictionary;
  dictionary & deviceProperties = *pDeviceProperties;
  // const char *value, *key;
  // FOREACH_DEVICE_PROPERTY(device, key, value) deviceProperties[key] = value ;
  const char *key, *value;
  for (key = sd_device_get_property_first(device, &value);
       key != NULL;
       key = sd_device_get_property_next(device, &value))
  {
    deviceProperties[key] = value ;
  }

  // Done with `sd_device device`
  sd_device_unref(device);


  // Copy device properties from `deviceProperties` into `device_info`
#define getDeviceProperty(key,field) \
  do \
  if (1==deviceProperties.count(#key)) { \
    std::string deviceProperty(deviceProperties[#key]); \
    LOG(D3) << #key << " is " << deviceProperty; \
    safecopy(device_info.field, sizeof(device_info.field), (uint8_t *)deviceProperty.c_str(), strlen(deviceProperty.c_str())); \
  } while (0)

  LOG(D3) << "Device properties from os:";
  getDeviceProperty(ID_SERIAL_SHORT,serialNum) ;
  getDeviceProperty(ID_MODEL,modelNum) ;
  getDeviceProperty(ID_REVISION,firmwareRev) ;
  getDeviceProperty(ID_VENDOR,vendorID) ;


  // Special brute-force copy into `device_info.passwordSalt`
  memcpy(device_info.passwordSalt, device_info.serialNum, sizeof(device_info.passwordSalt));


  // Copy `device_info` fields into `interfaceDeviceIndentification` blob for special cases
  uint8_t * p = (uint8_t *)interfaceDeviceIdentification;
#define copyDeviceIdentificationField(field,size)                           \
  do { memcpy(p,device_info.field, size); p += size; } while (0)

  copyDeviceIdentificationField(vendorID,INQUIRY_VENDOR_IDENTIFICATION_Length);
  copyDeviceIdentificationField(modelNum,INQUIRY_PRODUCT_IDENTIFICATION_Length);
  copyDeviceIdentificationField(firmwareRev,INQUIRY_PRODUCT_REVISION_LEVEL_Length);

  std::string bus=deviceProperties["ID_BUS"];

  if (bus=="scsi") {
    device_info.devType = DEVICE_TYPE_SCSI;
  } else if (bus == "usb") {
    if (deviceProperties["ID_USB_DRIVER"]=="uas") {
      device_info.devType = DEVICE_TYPE_SAS;
//    } else if (deviceProperties["ID_USB_DRIVER"]=="usb-storage") {
//      device_info.devType = DEVICE_TYPE_NVME;
    }
  } else if (bus == "ata") {
    if (deviceProperties["ID_USB_DRIVER"]=="uas") {
      device_info.devType = DEVICE_TYPE_SATA;
    } else {
      device_info.devType = DEVICE_TYPE_ATA;
    }
  } else if (bus == "nvme") {
    device_info.devType = DEVICE_TYPE_NVME;
  }

  // Return properties dictionary both as in indication of success and for futher mischief
  return pDeviceProperties;
}



DtaDevLinuxDrive * DtaDevLinuxDrive::getDtaDevLinuxDrive(const char * devref,
                                                         DTA_DEVICE_INFO &device_info,
                                                         bool &accessDenied)
{
  OSDEVICEHANDLE osDeviceHandle = DtaDevLinuxDrive::openDeviceHandle(devref, accessDenied);
  if (INVALID_HANDLE_VALUE == osDeviceHandle || accessDenied) {
    if (accessDenied) {
      // LOG(E)  << "(In DtaDevLinuxDrive::getDtaDevLinuxDrive)" ;
      LOG(E)  << "You do not have permission to access the raw device " << devref << " in write mode" ;
      LOG(E)  << "Perhaps you might try to run as administrator" ;
    }
    return NULL;
  }

  DtaDevLinuxDrive* drive = NULL;
  InterfaceDeviceID interfaceDeviceIdentification;
  memset(interfaceDeviceIdentification, 0, sizeof(interfaceDeviceIdentification));
  LOG(D4) << devref << " driveParameters:";
  dictionary * maybeDriveParameters = getOSSpecificInformation(osDeviceHandle, devref, interfaceDeviceIdentification, device_info);
  DtaDevLinuxDrive::closeDeviceHandle(osDeviceHandle);

  if (maybeDriveParameters == NULL) {
//	  LOG(E) << "Failed to determine drive parameters for " << devref;
	  return NULL;
  }

  dictionary & driveParameters = *maybeDriveParameters;
  IFLOG(D4)
    for (const auto & pair : driveParameters) {
        LOG(D4) << pair.first << ":\"" << pair.second << "\"";
  }

#define trySubclass(variant) \
  if ((drive = DtaDevLinux##variant::getDtaDevLinux##variant(devref, device_info)) != NULL) \
  { \
	break; \
  } else

#define logSubclassFailed(variant) \
  LOG(D4) << "DtaDevLinux" #variant "::getDtaDevLinux" #variant "(\"" << devref << "\", device_info) returned NULL";

#define skipSubclass(variant) \
  LOG(D4) << "DtaDevLinux" #variant "::getDtaDevLinux" #variant "(\"" << devref << "\", device_info) unimplmented";


  // Create a subclass instance based on device_info.devType as determined by
  // getOSSpecificInformation.  Customizing code has device_info and
  // drive parameters available.
  // Note: Unlike MacOS and Windows, Linux does not have a "block storage" class of which SCSI and NVMe are subclasses
  //       So "Scsi" is used as a fallback instead.
  //
  std::string typeName=DtaDevTypeName(device_info.devType);
  LOG(D4) << "device_info.devType=" << device_info.devType << ":" << (std::string)DtaDevTypeName(device_info.devType);
  switch (device_info.devType) {
  case DEVICE_TYPE_SCSI:  // SCSI
  case DEVICE_TYPE_SAS:   // SCSI
    trySubclass(Scsi)
      break;


  case DEVICE_TYPE_USB:   // UAS SAT -- USB -> SCSI -> AT pass-through
    //  case DEVICE_TYPE_SATA:  // synonym
    if (!deviceNeedsSpecialAction(interfaceDeviceIdentification,avoidSlowSATATimeout)) {
      trySubclass(Sata);
    }
    if (!deviceNeedsSpecialAction(interfaceDeviceIdentification,avoidSlowSASTimeout)) {
      trySubclass(Scsi);
    }
    break;


  case DEVICE_TYPE_NVME:  // NVMe
    // TODO: Just hack by using Scsi for now.  BlockStorageDevice?
    if (deviceNeedsSpecialAction(interfaceDeviceIdentification, acceptPseudoDeviceImmediately)) {
      trySubclass(/*BlockStorageDevice*/ Scsi);   // TODO: hack  TODO: DtaDevLinuxBlockStorageDevice TODO!!
      break;
    }
    //    trySubclass(Nvme)   // TODO test
    trySubclass(/* BlockStorageDevice */ Scsi);   // TODO: hack
    break;


  case DEVICE_TYPE_ATA:   // SATA / PATA
    skipSubclass(Ata)    // TODO
      break;


  case DEVICE_TYPE_OTHER:
    //    LOG(E) << "Unimplemented device type " << devref;
    break;


  default:
    LOG(E) << "Unknown device type " << device_info.devType << " for " << devref;
    break;
  }

  delete &driveParameters;
  return drive ;
}
