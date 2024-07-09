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

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#pragma warning(disable: 4224) //C2224: conversion from int to char , possible loss of data
#pragma warning(disable: 4244) //C4244: 'argument' : conversion from 'uint16_t' to 'uint8_t', possible loss of data
#pragma comment(lib, "rpcrt4.lib")  // UuidCreate - Minimum supported OS Win 2000
#endif // Windows
// this resolve error of uuid

#include <iostream>
#include "log.h"
#include "DtaHashPwd.h"
#include "DtaOptions.h"
#include "DtaLexicon.h"
#include "DtaDevGeneric.h"
#include "DtaDevOpal1.h"
#include "DtaDevOpal2.h"
#include "DtaDevEnterprise.h"
#include "DtaAuthorizeSedutilExecution.h"
#include "Version.h"


using namespace std;

static void isValidSEDDisk(const char * devname)
{
  DtaDev * d;
  uint8_t result = DtaDev::getDtaDev(devname, d, true);
  if (result == DTAERROR_SUCCESS && d->isPresent()) {
    printf("%s", devname);
    if (d->isAnySSC())
      printf(" SED %s%s%s ", (d->isOpal1() ? "1" : "-"),
             (d->isOpal2() ? "2" : "-"), (d->isEprise() ? "E" : "-"));
    else
      printf("%s", " NO --- ");
    cout << d->getModelNum() << ":" << d->getFirmwareRev();
    cout << std::endl;
  }
  delete d;
}


static uint8_t hashvalidate(char * password, char *devname)
{
  vector <uint8_t> hash;
  DtaDev * d;
  uint8_t result = DtaDev::getDtaDev(devname, d);
  if (result != DTAERROR_SUCCESS)
    return result;
  //bool saved_flag = d->no_hash_passwords;
  d->no_hash_passwords = false; // force to hash
  hash.clear();
  LOG(D1) << "start hashing random password";
  DtaHashPwd(hash, password, d);

  printf("password: %s", password);

  printf("unhashed in hex: ");
  for (size_t i = 0; i < strlen(password); i++) printf("%02X",password[i]);
  printf("\n");

  printf("hashed password: ");
  for (size_t i = 2; i < hash.size(); i++) printf("%02X",hash.at(i));
  printf("\n");

  return result;
}

static uint8_t main_actions(int argc, char **argv, DtaDev *d, const DTA_OPTIONS &opts) {
  switch (opts.action) {
  case sedutiloption::initialSetup:
    LOG(D) << "Performing initial setup to use sedutil on drive " << argv[opts.device];
    return (d->initialSetup(argv[opts.password]));
  case sedutiloption::setup_SUM:
    LOG(D) << "Performing SUM setup on drive " << argv[opts.device];
    return (d->setup_SUM(opts.lockingrange,
                         (unsigned long long)atoll(argv[opts.lrstart]),
                         (unsigned long long)atoll(argv[opts.lrlength]),
                         argv[opts.password], argv[opts.newpassword]));

  case sedutiloption::setSIDPassword:
    LOG(D) << "Performing setSIDPassword " << argv[opts.device];;
    return d->setSIDPassword(argv[opts.password], argv[opts.newpassword]);

  case sedutiloption::setAdmin1Pwd:
    LOG(D) << "Performing setPAdmin1Pwd " << argv[opts.device];;
    return d->setPassword(argv[opts.password], (char *) "Admin1",
                          argv[opts.newpassword]);

  case sedutiloption::getmfgstate:
    LOG(D) << "get manufacture life cycle state " << argv[opts.device];;
    return d->getmfgstate();

  case sedutiloption::activate:
    LOG(D) << "activate LockingSP with MSID " << argv[opts.device];;
    return d->activate(argv[opts.password]);

  case sedutiloption::loadPBAimage:
    LOG(D) << "Loading PBA image " << argv[opts.pbafile] << " to " << argv[opts.device];
    return d->loadPBA(argv[opts.password], argv[opts.pbafile]);

  case sedutiloption::setLockingRange:
    LOG(D) << "Setting Locking Range " <<
      (uint16_t) opts.lockingrange << " " <<
      (uint16_t) opts.lockingstate << " " <<
      argv[opts.device];;
    return d->setLockingRange(opts.lockingrange, opts.lockingstate, argv[opts.password]);

  case sedutiloption::setLockingRange_SUM:
    LOG(D) << "Setting Locking Range " << (uint16_t)opts.lockingrange << " "
           << (uint16_t)opts.lockingstate << " in Single User Mode";
    return d->setLockingRange_SUM(opts.lockingrange, opts.lockingstate, argv[opts.password]);

  case sedutiloption::enableLockingRange:
    LOG(D) << "Enabling Locking Range " << (uint16_t) opts.lockingrange << " " << argv[opts.device];
    return (d->configureLockingRange(opts.lockingrange,
                                     (DTA_READLOCKINGENABLED | DTA_WRITELOCKINGENABLED), argv[opts.password]));

  case sedutiloption::disableLockingRange:
    LOG(D) << "Disabling Locking Range " << (uint16_t) opts.lockingrange << " " << argv[opts.device];
    return (d->configureLockingRange(opts.lockingrange, DTA_DISABLELOCKING,
                                     argv[opts.password]));

  case sedutiloption::readonlyLockingRange:
    LOG(D) << "Enabling Locking Range " << (uint16_t)opts.lockingrange << " " << argv[opts.device];
    return (d->configureLockingRange(opts.lockingrange,
                                     DTA_WRITELOCKINGENABLED, argv[opts.password]));

  case sedutiloption::setupLockingRange:
    LOG(D) << "Setup Locking Range " << (uint16_t)opts.lockingrange << " " << argv[opts.device];
    return (d->setupLockingRange(opts.lockingrange,
                                 (unsigned long long)atoll(argv[opts.lrstart]),
                                 (unsigned long long)atoll(argv[opts.lrlength]),
                                 argv[opts.password]));

  case sedutiloption::setupLockingRange_SUM:
    LOG(D) << "Setup Locking Range " << (uint16_t)opts.lockingrange <<
      " in Single User Mode " << argv[opts.device];
    return (d->setupLockingRange_SUM(opts.lockingrange,
                                     (unsigned long long)atoll(argv[opts.lrstart]),
                                     (unsigned long long)atoll(argv[opts.lrlength]),
                                     argv[opts.password]));

  case sedutiloption::listLockingRanges:
    LOG(D) << "List Locking Ranges " << argv[opts.device];
    return (d->listLockingRanges(argv[opts.password], -1));

  case sedutiloption::listLockingRange:
    LOG(D) << "List Locking Range[" << opts.lockingrange << "] " << argv[opts.device];
    return (d->listLockingRanges(argv[opts.password], opts.lockingrange));

  case sedutiloption::rekeyLockingRange:
    LOG(D) << "Rekey Locking Range[" << opts.lockingrange << "] " << argv[opts.device];
    return (d->rekeyLockingRange(opts.lockingrange, argv[opts.password]));

  case sedutiloption::setBandsEnabled:
    LOG(D) << "Set bands Enabled " << argv[opts.device];
    return (d->setBandsEnabled(-1, argv[opts.password]));

  case sedutiloption::setBandEnabled:
    LOG(D) << "Set band[" << opts.lockingrange << "] enabled " << argv[opts.device];
    return (d->setBandsEnabled(opts.lockingrange, argv[opts.password]));

  case sedutiloption::setMBRDone:
    LOG(D) << "Setting MBRDone " << (uint16_t)opts.mbrstate << " " << argv[opts.device];
    return (d->setMBRDone(opts.mbrstate, argv[opts.password]));

  case sedutiloption::setMBREnable:
    LOG(D) << "Setting MBREnable " << (uint16_t)opts.mbrstate << " " << argv[opts.device];
    return (d->setMBREnable(opts.mbrstate, argv[opts.password]));

  case sedutiloption::enableuser:
    LOG(D) << "Performing enable user for user " << argv[opts.userid] << " " << argv[opts.device];
    return d->enableUser(opts.mbrstate, argv[opts.password], argv[opts.userid]);

  case sedutiloption::enableuserread:
    LOG(D) << "Performing enable user for user " << argv[opts.userid] << " " << argv[opts.device];
    return d->enableUserRead(opts.mbrstate, argv[opts.password], argv[opts.userid]);

  case sedutiloption::activateLockingSP:
    LOG(D) << "Activating the LockingSP on " << argv[opts.device];
    return d->activateLockingSP(argv[opts.password]);

  case sedutiloption::activateLockingSP_SUM:
    LOG(D) << "Activating the LockingSP on" << argv[opts.device] << " " << argv[opts.device];
    return d->activateLockingSP_SUM(opts.lockingrange, argv[opts.password]);

  case sedutiloption::eraseLockingRange_SUM:
    LOG(D) << "Erasing LockingRange " << opts.lockingrange <<
      " on" << argv[opts.device] << " " << argv[opts.device];
    return d->eraseLockingRange_SUM(opts.lockingrange, argv[opts.password]);

  case sedutiloption::query:
    LOG(D) << "Performing diskquery() on " << argv[opts.device];
    d->puke();
    return 0;

  case sedutiloption::scan:
    LOG(D) << "Performing diskScan() ";
    return(DtaDevOS::diskScan());

  case sedutiloption::isValidSED:
    LOG(D) << "Verify whether " << argv[opts.device] << "is valid SED or not";
    isValidSEDDisk(argv[opts.device]);
    return 0;

  case sedutiloption::takeOwnership:
    LOG(D) << "Taking Ownership of the drive at " << argv[opts.device];
    return d->takeOwnership(argv[opts.password]);

  case sedutiloption::revertLockingSP:
    LOG(D) << "Performing revertLockingSP on " << argv[opts.device];
    return d->revertLockingSP(argv[opts.password], 0);

  case sedutiloption::setPassword:
    LOG(D) << "Performing setPassword for user " << argv[opts.userid] << " " << argv[opts.device];;
    return d->setPassword(argv[opts.password], argv[opts.userid],
                          argv[opts.newpassword]);

  case sedutiloption::setPassword_SUM:
    LOG(D) << "Performing setPassword in SUM mode for user " << argv[opts.userid] << " " << argv[opts.device];
    return d->setNewPassword_SUM(argv[opts.password], argv[opts.userid],
                                 argv[opts.newpassword]);

  case sedutiloption::revertTPer:
    LOG(D) << "Performing revertTPer on " << argv[opts.device];
    return d->revertTPer(argv[opts.password], 0, 0);

  case sedutiloption::revertNoErase:
    LOG(D) << "Performing revertLockingSP  keep global locking range on " << argv[opts.device];
    return d->revertLockingSP(argv[opts.password], 1);

  case sedutiloption::validatePBKDF2:
    LOG(D) << "Performing PBKDF2 validation ";
    return TestPBKDF2();

  case sedutiloption::yesIreallywanttoERASEALLmydatausingthePSID:
  case sedutiloption::PSIDrevert:
    LOG(D) << "Performing a PSID Revert on " << argv[opts.device] <<
      " with password " << argv[opts.password] << " " << argv[opts.device];
    return d->revertTPer(argv[opts.password], 1, 0);

  case sedutiloption::PSIDrevertAdminSP:
    LOG(D) << "Performing a PSID RevertAdminSP on " << argv[opts.device] <<
      " with password " << argv[opts.password] << " " << argv[opts.device];
    return d->revertTPer(argv[opts.password], 1, 1);

  case sedutiloption::eraseLockingRange:
    LOG(D) << "Erase Locking Range " << (uint16_t)opts.lockingrange << " " << argv[opts.device];
    return (d->eraseLockingRange(opts.lockingrange, argv[opts.password]));

  case sedutiloption::objDump:
    LOG(D) << "Performing objDump " ;
    return d->objDump(argv[argc - 5], argv[argc - 4], argv[argc - 3], argv[argc - 2]);

  case sedutiloption::printDefaultPassword:
    LOG(D) << "print default password";
    return d->printDefaultPassword();

  case sedutiloption::rawCmd:
    LOG(D) << "Performing cmdDump ";
    return d->rawCmd(argv[argc - 7], argv[argc - 6], argv[argc - 5], argv[argc - 4], argv[argc - 3], argv[argc - 2]);

  case sedutiloption::hashvalidation:
    LOG(D) << "Hash Validation";
    return hashvalidate(argv[opts.password],argv[opts.device]);

  case sedutiloption::TCGreset:
    LOG(D) << "TCG Reset " <<  " " << argv[opts.device];
    return (d->TCGreset(opts.resettype));


#include "Customizations/DtaExtensionOptionImplementations.inc"

  default:
    LOG(E) << "Unable to determine what you want to do ";
    usage();
    return DTAERROR_INVALID_COMMAND;
  }
}

int main(int argc, char * argv[])
{
  DTA_OPTIONS opts;
  uint8_t result = DtaOptions(argc, argv, &opts);
  if (DTAERROR_SUCCESS!=result) {
    return result;
  }

  if (! authorize_sedutil_execution(argc, argv)) {
    return DTAERROR_AUTHORIZE_EXEC_FAILED;
  }

  if (0 == opts.device) {
    return main_actions(argc, argv, NULL, opts);
  }

  if (argc <= opts.device) {
    return DTAERROR_INVALID_COMMAND;
  }

  DtaDev *d = NULL;
  result = DtaDev::getDtaDev(argv[opts.device], d);
  if (result == DTAERROR_SUCCESS && d!= NULL) {
  // initialize instance variables no_hash_passwords and output_format
    d->no_hash_passwords = opts.no_hash_passwords;
    d->output_format = opts.output_format;
    result = main_actions(argc, argv, d, opts);
    delete d;
  }

  return result;
}
