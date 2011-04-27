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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

var Pref = {
    chooseFolder: function () {
        var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
        var bundlePreferences = document.getElementById("bundlePreferences");
        var title = bundlePreferences.getString("chooseBookshelfFolderTitle");
        fp.init(window, title, Ci.nsIFilePicker.modeGetFolder);
        fp.appendFilters(Ci.nsIFilePicker.filterAll);
        var currentDirPref = document.getElementById("chmsee.bookshelf.dir");

        fp.displayDirectory = currentDirPref.value;

        if (fp.show() == Ci.nsIFilePicker.returnOK) {
            currentDirPref.value = fp.file;
        }
    },

    displayBookshelfDir: function () {
        var bookshelfFolder = document.getElementById("bookshelfFolder");
        var currentDirPref = document.getElementById("chmsee.bookshelf.dir");

        if (!currentDirPref.value) {
            var dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
            var homeDir = dirService.get("Home", Ci.nsIFile);
            var path = path = homeDir.path + "/.chmsee/bookshelf";
            var bookshelfDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
            bookshelfDir.initWithPath(path);
            currentDirPref.value = bookshelfDir;
        }

        bookshelfFolder.label = currentDirPref.value.path;
        bookshelfFolder.image = "moz-icon://stock/gtk-directory?size=16";
    
        return undefined;
    },

};
