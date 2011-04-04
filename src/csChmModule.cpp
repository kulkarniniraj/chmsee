/*
 *  Copyright (C) 2011 Ji YongGang <jungleji@gmail.com>
 *
 *  ChmSee is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.

 *  ChmSee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with ChmSee; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "csChm.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(csChm)

NS_DEFINE_NAMED_CID(CS_CHM_CID);

static const mozilla::Module::CIDEntry kcsChmCIDs[] = {
        { &kCS_CHM_CID, false, NULL, csChmConstructor },
        { NULL }
};

static const mozilla::Module::ContractIDEntry kcsChmContracts[] = {
        { CS_CHM_CONTRACTID, &kCS_CHM_CID },
        { NULL }
};

static const mozilla::Module kcsChmModule = {
        mozilla::Module::kVersion,
        kcsChmCIDs,
        kcsChmContracts
};

NSMODULE_DEFN(nscsChmModule) = &kcsChmModule;
