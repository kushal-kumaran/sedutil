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

#include <algorithm>
#include "dirent.h"

#include "DtaDevOSDrive.h"
#include "DtaDevLinuxDrive.h"
#include "DtaDevLinuxNvme.h"
#include "DtaDevLinuxScsi.h"
#include "ParseDiscovery0Features.h"


/** Factory functions
 *
 * Static class members of DtaDevOSDrive that are passed through
 * to DtaDevLinuxDrive
 *
 */

bool DtaDevOSDrive::isDtaDevOSDriveDevRef(const char * devref, bool & accessDenied) {
  // LOG(E) << "In isDtaDevOSDriveDevRef: devref=\""<< devref << "\"" ;        // TODO: debugging
  bool result = DtaDevLinuxDrive::isDtaDevLinuxDriveDevRef(devref);
  // LOG(E) << "result = DtaDevLinuxDrive::isDtaDevLinuxDriveDevRef(\""
  //        << devref << "\") = " << std::boolalpha << result;                 // TODO: debugging
  if (result) {
    OSDEVICEHANDLE osDeviceHandle = openDeviceHandle(devref, accessDenied);
    // LOG(E) << "In isDtaDevOSDriveDevRef: OSDEVICEHANDLE osDeviceHandle = " << HEXON(16) << osDeviceHandle
    //        << " = openDeviceHandle(\"" << devref << "\", "
    //                             << std::boolalpha << accessDenied << ")";  // TODO: debugging
    result = (osDeviceHandle!=INVALID_HANDLE_VALUE && !accessDenied);
    // LOG(E) << "result = " << std::boolalpha << result;                     // TODO: debugging
    if (osDeviceHandle!=INVALID_HANDLE_VALUE) closeDeviceHandle(osDeviceHandle);
  }
  return result;
}

std::vector<std::string> DtaDevOSDrive::enumerateDtaDevOSDriveDevRefs(bool & accessDenied) {
    return DtaDevLinuxDrive::enumerateDtaDevLinuxDriveDevRefs(accessDenied);
}

DtaDevOSDrive * DtaDevOSDrive::getDtaDevOSDrive(const char * devref,
                                                DTA_DEVICE_INFO &device_info,
                                                bool& accessDenied)
{
  DtaDevLinuxDrive * drive=DtaDevLinuxDrive::getDtaDevLinuxDrive(devref, device_info, accessDenied);
  return static_cast<DtaDevOSDrive *>(drive);
}


OSDEVICEHANDLE DtaDevOSDrive::openDeviceHandle(const char* devref, bool& accessDenied) {
  return DtaDevLinuxDrive::openDeviceHandle(devref, accessDenied);
}

void DtaDevOSDrive::closeDeviceHandle(OSDEVICEHANDLE osDeviceHandle) {
  LOG(D4) << "Entering DtaDevOSDrive::closeDeviceHandle";
  DtaDevLinuxDrive::closeDeviceHandle(osDeviceHandle);
  LOG(D4) << "Exiting DtaDevOSDrive::closeDeviceHandle";
}



bool DtaDevLinuxDrive::isDtaDevLinuxDriveDevRef(const char * devref)
{
  return DtaDevLinuxNvme::isDtaDevLinuxNvmeDevRef(devref)
    ||   DtaDevLinuxScsi::isDtaDevLinuxScsiDevRef(devref) ;
}


OSDEVICEHANDLE DtaDevLinuxDrive::openAndCheckDeviceHandle(const char * devref, bool& accessDenied)
{
  if (isDtaDevLinuxDriveDevRef(devref)) {
    return INVALID_HANDLE_VALUE;
  }

  if (access(devref, R_OK | W_OK)) {
    accessDenied = true;
    return INVALID_HANDLE_VALUE;
  }

  accessDenied = false;
  OSDEVICEHANDLE osDeviceHandle = openDeviceHandle(devref, accessDenied);

  if (accessDenied) {
    return INVALID_HANDLE_VALUE;
  }

  int32_t descriptor = handleDescriptor(osDeviceHandle);
  if (descriptor < 0) {
    LOG(E) << "Error "  << (int32_t) descriptor << " opening device " << devref;
    if (-EPERM == descriptor) {
      LOG(E)  << "(From DtaDevLinuxDrive::openAndCheckDeviceHandle:)" ;
      LOG(E)  << "You do not have permission to access the raw device " << devref << " in write mode" ;
      LOG(E)  << "Perhaps you might try to run as administrator" ;
      accessDenied = true;
    }
    return INVALID_HANDLE_VALUE;
  }

  return osDeviceHandle;
}


OSDEVICEHANDLE DtaDevLinuxDrive::openDeviceHandle(const char* devref, bool & accessDenied) {
  int descriptor=open(devref, O_RDWR);

  if (descriptor == -1) {
    switch (errno) {
    case EACCES:
      accessDenied = true;
      break;
    case ENOENT:
      LOG(E) << "No such device: " << devref;
      break;
    default:
      LOG(E) << "Failed opening " << devref << " : " << strerror(errno) ;
      break;
    }
    return INVALID_HANDLE_VALUE;
  }

  return handle(descriptor);
}

void DtaDevLinuxDrive::closeDeviceHandle(OSDEVICEHANDLE osDeviceHandle) {
  LOG(D4) << "Entering DtaDevLinuxDrive::closeDeviceHandle";
  int descriptor = handleDescriptor(osDeviceHandle);
  LOG(D4) << "DtaDevLinuxDrive::closeDeviceHandle -- calling close(" << descriptor << ")...";
  close(descriptor);
  LOG(D4) << "DtaDevLinuxDrive::closeDeviceHandle -- returned from close(" << descriptor << ")";
  LOG(D4) << "Exiting DtaDevLinuxDrive::closeDeviceHandle";
}

std::vector<std::string> DtaDevLinuxDrive::enumerateDtaDevLinuxDriveDevRefs(bool & accessDenied)
{
    std::vector<std::string> devrefs;

  DIR *dir = opendir("/dev");
  if (dir==NULL) {
    LOG(E) << "Can't read /dev ?!";
    return devrefs;
  }

  struct dirent *dirent;
  while (NULL != (dirent=readdir(dir))) {
    std::string str_devref=std::string("/dev/")+dirent->d_name;
    const char * devref=str_devref.c_str();
    bool accessDeniedThisTime=false;
    if (isDtaDevOSDriveDevRef(devref, accessDeniedThisTime))
      devrefs.push_back(str_devref);
    else if (accessDeniedThisTime) {
      accessDenied=true;
    }
  }

  closedir(dir);

  std::sort(devrefs.begin(),devrefs.end());

  return devrefs;
}

uint8_t DtaDevLinuxDrive::discovery0(DTA_DEVICE_INFO & disk_info) {
  void * d0Response = alloc_aligned_MIN_BUFFER_LENGTH_buffer();
  if (d0Response == NULL)
      return DTAERROR_COMMAND_ERROR;
  memset(d0Response, 0, MIN_BUFFER_LENGTH);

  int lastRC = sendCmd(IF_RECV, 0x01, 0x0001, d0Response, MIN_BUFFER_LENGTH);
  if ((lastRC ) != 0) {
    LOG(D4) << "Acquiring Discovery 0 response failed " << lastRC;
    return DTAERROR_COMMAND_ERROR;
  }
  parseDiscovery0Features((uint8_t *)d0Response, disk_info);
  free_aligned_MIN_BUFFER_LENGTH_buffer(d0Response);
  return DTAERROR_SUCCESS;
}
