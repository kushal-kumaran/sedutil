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
#pragma once
#include <fcntl.h>
#include <errno.h>
#include "DtaStructures.h"

/** virtual implementation for a disk interface-generic disk drive
 */
class DtaDevOSDrive {
public:
  /** Factory function to look at the devref and create an instance of the appropriate subclass of
   *  DtaDevOSDrive
   *
   * @param devref OS device reference e.g. "/dev/sda"
   * @param pdisk_info weak reference to DTA_DEVICE_INFO structure filled out during device identification
   */
  static DtaDevOSDrive * getDtaDevOSDrive(const char * devref,
                                                DTA_DEVICE_INFO & disk_info);



  /** Method to send a command to the device
   * @param cmd command to be sent to the device
   * @param protocol security protocol to be used in the command
   * @param comID communications ID to be used
   * @param buffer input/output buffer
   * @param bufferlen length of the input/output buffer
   */
  virtual uint8_t sendCmd(ATACOMMAND cmd, uint8_t protocol, uint16_t comID,
                          void * buffer, uint32_t bufferlen) = 0;

  /** Routine to identify the device and fill out the device info structure.
   *
   * If it is an ATA device, perform an ATA Identify,
   * or if SCSI (SAS) , perform a SCSI Inquiry,
   * or if NVME, perform an NVMe Identify,
   * and perform a call to discovery0() to complete the disk_info structure
   * @param disk_info reference to the device info structure to fill out
   */
  virtual bool identify(DTA_DEVICE_INFO& disk_info) = 0;

  uint8_t discovery0(DTA_DEVICE_INFO & di);

  bool isOpen(void) {return 0<fd && (fcntl(fd, F_GETFL) != -1 || errno != EBADF);}

  DtaDevOSDrive(int _fd) :fd(_fd) {}

  virtual ~DtaDevOSDrive() {fdclose();}


protected:

  static int fdopen(const char * devref);
  void fdclose(void);

public:  // *** TODO *** DEBUGGING *** this should just be protected
  int fd=0; /**< Linux handle for the device  */

// private:
//   DtaDevOSDrive(){};

};




template <typename T>
static inline void safecopy(T * dst, size_t dstsize, const T * src, size_t srcsize)
{
  const T *p=src, *p_end=p+srcsize;
  while ((T)(0)==(*p)) if (p_end==++p) return;  // Do not erase dst if src is all zeros

  if (dstsize<=srcsize)
    memcpy(dst,src,dstsize);
  else {
    memcpy(dst,src,srcsize);
    memset(dst+srcsize, (T)(0), dstsize-srcsize);
  }
}
