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

var EXPORTED_SYMBOLS = ["d", "startDOMi"];

/*** Debug ***/

const CsDebug = true;

var d = function (f, s) {
    if (CsDebug)
        dump(f + " >>> " + s + "\n");
}

// Call DOM inspector for debugging
var startDOMi = function () {
    // Load the Window DataSource so that browser windows opened subsequent to DOM
    // Inspector show up in the DOM Inspector's window list.
    var windowDS = Cc["@mozilla.org/rdf/datasource;1?name=window-mediator"].getService(Ci.nsIWindowDataSource);
    var tmpNameSpace = {};
    var sl = Cc["@mozilla.org/moz/jssubscript-loader;1"].createInstance(Ci.mozIJSSubScriptLoader);
    sl.loadSubScript("chrome://inspector/content/hooks.js", tmpNameSpace);
    tmpNameSpace.inspectDOMDocument(document);
}
