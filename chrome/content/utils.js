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

var EXPORTED_SYMBOLS = ["Pref", "d", "CsScheme"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

var CsScheme = "file://";

/*** Read/Save preference ***/

const prefService = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

var Pref = {
    getLastDir: function () {
        var dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
        var lastDir = dirService.get("Home", Ci.nsIFile);

        if (prefService.getPrefType("chmsee.lastdir") == prefService.PREF_STRING) {
            var lastPath = prefService.getCharPref("chmsee.lastdir");
            lastDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
            lastDir.initWithPath(lastPath);
        }

        return lastDir;
    },

    saveLastDir: function (lastDir) {
        prefService.setCharPref("chmsee.lastdir", lastDir);
    },
};

/*** Debug ***/

const CsDebug = true;

var d = function (f, s) {
    if (CsDebug)
        dump(f + " >>> " + s + "\n");
};
