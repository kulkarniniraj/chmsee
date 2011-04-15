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

var contentTabbox = null;

/*** Event handlers ***/

var onWindowLoad = function () {
    d("onWindowLoad", "init");

    initTabbox();
};

var onTocSelected = function (event) {
    var tree = event.target;
    var cellIndex = 1;
    var cellText = tree.view.getCellText(tree.currentIndex, tree.columns.getColumnAt(cellIndex));

    var browser = tree.browser;
    var url = "chmsee://" + cellText;
    d("onTocSelected", "url = " + url);
    browser.setAttribute("src", url);
};

/*** Commands ***/

var openFile = function () {
    var nsIFilePicker = Ci.nsIFilePicker;
    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);

    var strbundle = document.getElementById("bundle-main");
    var strSelectFile=strbundle.getString("selectChmFile");
    var strChmFile=strbundle.getString("chmFile");

    fp.init(window, strSelectFile, nsIFilePicker.modeOpen);
    fp.displayDirectory = Pref.getLastDir();
    fp.appendFilter(strChmFile, "*.chm;*.CHM");
    fp.appendFilters(nsIFilePicker.filterAll);

    // Popup open file dialog
    var res = fp.show();

    if (res == nsIFilePicker.returnOK) {
        d("openFile", "selected file path = " + fp.file.path);
        Pref.saveLastDir(fp.file.parent.path);

        var book = Book.getBookFromFile(fp.file);

        var newTab = createBookTab(book);
        replaceTab(newTab, getCurrentTab());
        refreshBookTab(newTab);
    }
};

var newTab = function () {
    var currentPanel = contentTabbox.selectedPanel;

    var index = -1;

    try {
    if (currentPanel.type === "book") {
        var newTab = createBookTab(currentPanel.book);
        appendTab(newTab);
        refreshBookTab(newTab);
        index = contentTabbox.tabs.itemCount - 1;
    } else if (currentPanel.type === "welcome") {
        appendTab(createWelcomeTab());
        index = contentTabbox.tabs.itemCount - 1;
    } else {
        index = 0;
    }
    } catch (e) {
        alert (e);
    }

    contentTabbox.selectedIndex = index;
};

var closeTab = function () {
    if (contentTabbox.tabs.itemCount === 1)
        return;

    removeTab(getCurrentTab());
};

/*** Other functions ***/

var initTabbox = function () {
    contentTabbox = document.getElementById("content-tabbox");
    appendTab(createWelcomeTab());
    contentTabbox.selectedIndex = 0;
};

var createWelcomeTab = function () {
    var book = Book.getBookFromUrl("Welcome", "about:mozilla");

    var tab = document.createElement("tab");
    tab.setAttribute("label", book.title);

    var panel = document.createElement("tabpanel");
    var browser = document.createElement("iframe");
    browser.setAttribute("flex", 1);
    browser.setAttribute("src", book.homepage);
    panel.appendChild(browser);

    panel.book = book;
    panel.type = "welcome";

    return {tab: tab, panel: panel};
};

var createBookTab = function (book) {
    var bookTab = document.createElement("tab");
    bookTab.setAttribute("label", book.title);

    var bookPanel = document.createElement("tabpanel");
    bookPanel.setAttribute("orient", "vertical");
    bookPanel.setAttribute("flex", "1");

    var bookPanelBox = document.createElement("vbox");
    bookPanelBox.setAttribute("flex", "1");
    bookPanel.appendChild(bookPanelBox);

    var bookToolbox = document.createElement("toolbox");
    bookToolbox.setAttribute("class", "book-toolbox");
    bookPanelBox.appendChild(bookToolbox);

    var bookContentBox = document.createElement("hbox");
    bookContentBox.setAttribute("flex", "1");
    bookPanelBox.appendChild(bookContentBox);

    var treeTabbox = document.createElement("tabbox");
    var treeTabs = document.createElement("tabs");
    treeTabbox.appendChild(treeTabs);
    var treePanels = document.createElement("tabpanels");
    treePanels.setAttribute("flex", "1");
    treeTabbox.appendChild(treePanels);

    if (book.hhcDS !== null) {
        var toc = createTreeTab("Toc");
        treeTabs.appendChild(toc.tab);
        treePanels.appendChild(toc.panel);
        treeTabbox.toc = toc.treeBox;
    }

    if (book.hhkDS !== null) {
        var index = createTreeTab("Index");
        treeTabs.appendChild(index.tab);
        treePanels.appendChild(index.panel);
        treeTabbox.index = index.treeBox;
    }

    bookContentBox.appendChild(treeTabbox);

    var splitter = document.createElement("splitter");
    splitter.setAttribute("collapse", "before");
    splitter.appendChild(document.createElement("grippy"));
    bookContentBox.appendChild(splitter);

    // XUL browser element only support standard scheme
    var browser = document.createElement("iframe");
    browser.setAttribute("src", book.url);
    browser.setAttribute("flex", "1");
    bookContentBox.appendChild(browser);

    bookPanel.type = "book";
    bookPanel.book = book;
    bookPanel.treebox = treeTabbox;
    bookPanel.spliter = splitter;
    bookPanel.browser = browser;

    return {tab: bookTab, panel: bookPanel};
};

var createTreeTab = function (title) {
    var tab = document.createElement("tab");
    tab.setAttribute("label", title);

    var panel = document.createElement("tabpanel");
    var treeBox = document.createElement("box");
    treeBox.setAttribute("class", "book-treebox");
    treeBox.setAttribute("flex", 1);
    panel.appendChild(treeBox);

    return {tab: tab, panel: panel, treeBox: treeBox};
};

var appendTab = function (tab) {
    contentTabbox.tabs.appendChild(tab.tab);
    contentTabbox.tabpanels.appendChild(tab.panel);
};

var replaceTab = function (newTab, oldTab) {
    var currentIndex = contentTabbox.selectedIndex;

    contentTabbox.tabs.replaceChild(newTab.tab, oldTab.tab);
    contentTabbox.tabpanels.replaceChild(newTab.panel, oldTab.panel);

    if (currentIndex !== contentTabbox.selectedIndex)
        contentTabbox.selectedIndex = currentIndex;
};

var removeTab = function (tab) {
    var tabs = contentTabbox.tabs;
    var tabpanels = contentTabbox.tabpanels;
    var currentIndex = contentTabbox.selectedIndex;

    tabs.removeChild(tab.tab);
    tabpanels.removeChild(tab.panel);

    if (tab.index === currentIndex && tabs.itemCount > 1)
        contentTabbox.selectedIndex = tab.index -1;

    if (contentTabbox.selectedIndex === -1)
        contentTabbox.selectedIndex = 0;
};

var refreshBookTab = function (tab) {
    var panel = tab.panel;
    var book = panel.book;
    var treebox = panel.treebox;
    var splitter = panel.splitter;

    if (treebox.toc)
        var tocTree = treebox.toc.tree;
    if (treebox.index)
        var indexTree = treebox.index.tree;

    if (book.hhcDS !== null && tocTree) {
        tocTree.database.AddDataSource(book.hhcDS);
        tocTree.builder.rebuild();
        tocTree.browser = panel.browser;
    }

    if (book.hhkDS !== null && indexTree) {
        indexTree.database.AddDataSource(book.hhkDS);
        indexTree.builder.rebuild();
        indexTree.browser = panel.browser;
    }

    if (treebox.tabs.itemCount === 1)
        treebox.tabs.hidden = true;

    if (treebox.tabs.itemCount === 0) {
        treebox.hidden = true;
        splitter.hidden = true;
    }
};

var getCurrentTab = function () {
    return { index: contentTabbox.selectedIndex,
             tab: contentTabbox.selectedTab,
             panel: contentTabbox.selectedPanel };
};

// Call DOM inspector for debugging
var startDOMi = function () {
    // Load the Window DataSource so that browser windows opened subsequent to DOM
    // Inspector show up in the DOM Inspector's window list.
    var windowDS = Cc["@mozilla.org/rdf/datasource;1?name=window-mediator"].getService(Ci.nsIWindowDataSource);
    var tmpNameSpace = {};
    var sl = Cc["@mozilla.org/moz/jssubscript-loader;1"].createInstance(Ci.mozIJSSubScriptLoader);
    sl.loadSubScript("chrome://inspector/content/hooks.js", tmpNameSpace);
    tmpNameSpace.inspectDOMDocument(document);
};
