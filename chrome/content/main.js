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

Cu.import("chrome://chmsee/content/utils.js");
Cu.import("chrome://chmsee/content/book.js");

/*** Event handlers ***/

function onWindowLoad() {
    d("onWindowLoad", "init");

    initTabbox();
}

function onTocSelected(panel) {
    var tree = panel.toc;
    var cellIndex = 1;
    var cellText = tree.view.getCellText(tree.currentIndex, tree.columns.getColumnAt(cellIndex));
    d("onTocSelected", "cellText: " + cellText);

    var browser = panel.browser;
    var url = "chmsee://" + cellText;
    d("onTocSelected", "browser = " + browser.tagName + "url = " + url);
    panel.setAttribute("url", url);
}

/*** Commands ***/

function openFile() {
    // Read last opened directory
    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    var dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
    var lastDir = dirService.get("Home", Ci.nsIFile);

    if (prefs.getPrefType("chmsee.lastdir") == prefs.PREF_STRING) {
        var lastPath = prefs.getCharPref("chmsee.lastdir");
        lastDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
        lastDir.initWithPath(lastPath);
    }

    // Popup file open dialog
    var nsIFilePicker = Ci.nsIFilePicker;
    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);

    var strbundle = document.getElementById("bundle-main");
    var strSelectFile=strbundle.getString("selectChmFile");
    var strChmFile=strbundle.getString("chmFile");

    fp.init(window, strSelectFile, nsIFilePicker.modeOpen);
    fp.displayDirectory = lastDir;
    fp.appendFilter(strChmFile, "*.chm;*.CHM");
    fp.appendFilters(nsIFilePicker.filterAll);

    var res = fp.show();

    if (res == nsIFilePicker.returnOK) {
        d("openFile", "selected file path = " + fp.file.path);
        prefs.setCharPref("chmsee.lastdir", fp.file.parent.path);

        var book = Book.getBookFromFile(fp.file);
        d("openFile", "book fp.file = " + fp.file);

        updateTab(book);
    }
}

function newTab() {
    var tabbox = document.getElementById("content-tabbox");

    var currentTabLabel = tabbox.selectedTab.getAttribute("label");
    var currentUrl = tabbox.selectedPanel.getAttribute("url");
    var currentBook = tabbox.selectedPanel.book;

    var newIndex = createTab();
    tabbox.selectedIndex = newIndex;

    updateTab(currentBook);
}

function closeTab() {
    var tabbox = document.getElementById("content-tabbox");
    var tabs = tabbox.tabs;
    var tabpanels = tabbox.tabpanels;
    var currentIndex = tabbox.selectedIndex;
    d("closeTab", "selected index = " + currentIndex + ", tabindex = " + tabs.selectedIndex);

    if (currentIndex == 0 && tabs.itemCount == 1 ){
        return;
    } else {
        tabs.removeChild(tabbox.selectedTab);
        tabpanels.removeChild(tabbox.selectedPanel);

        if (tabbox.selectedIndex == -1 && tabs.itemCount > 0){
            tabbox.selectedIndex = currentIndex -1;
        }
    }
}


function initTabbox() {
    var tabbox = document.getElementById("content-tabbox");

    if (tabbox.tabs == null)
        tabbox.appendChild(document.createElement("tabs"));

    if (tabbox.tabpanels == null)
        tabbox.appendChild(document.createElement("tabpanels"));

    tabbox.tabpanels.setAttribute("flex", "1");

    var newIndex = createTab();
    tabbox.selectedIndex = newIndex;

    d("initTabbox", "0");
    var book = Book.getBookFromUrl("Starter", "about:mozilla");
    d("initTabbox", "1");
    updateTab(book);
    d("initTabbox", "2");
}

function createTab() {
    var tabbox = document.getElementById("content-tabbox");
    var tab = document.createElement("tab");
    // tab.setAttribute("class", "book-tab");
    tabbox.tabs.appendChild(tab);

    var panel = document.createElement("tabpanel");
    panel.setAttribute("class", "book-tabpanel");
    tabbox.tabpanels.appendChild(panel);

    return tabbox.tabs.itemCount - 1;
}

function updateTab(book) {
    var tabbox = document.getElementById("content-tabbox");
    var tab = tabbox.selectedTab;
    tab.setAttribute("label", book.title);

    var panel = tabbox.selectedPanel;
    // panel.browser.loadURI(book.url);
    panel.setAttribute("url", book.url);
    panel.book = book;

    if (book.hhcDS != null && book.hhkDS != null)
        panel.setAttribute("hiddenTocIndex", "false");
    else
        panel.setAttribute("hiddenTocIndex", "true");

    if (book.hhcDS) {
        panel.setTree(book.hhcDS);
        panel.setAttribute("splitter", "open");
    } else
        panel.setAttribute("splitter", "collapsed");
}

function goHome(panel) {
    d("goHome", "brower homepage = " + panel.browser.getAttribute("homepage"));
    panel.browser.goHome();
}

function goBack(panel) {
    d("goBack", "brower tag = " + panel.browser.tagName);
    panel.browser.goBack();
}

function goForward(panel) {
    d("goForward", "brower tag = " + panel.browser.tagName);
    panel.browser.goForward();
}

function tocClick(panel) {
    d("tocClick", "");
    var book = panel.book;
    if (book.hhcDS != null)
        panel.setTree(book.hhcDS);
}

function indexClick(panel) {
    d("indexClick", "");
    var book = panel.book;
    if (book.hhkDS != null)
        panel.setTree(book.hhkDS);
}
