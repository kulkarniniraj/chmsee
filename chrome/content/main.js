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

    window.addEventListener("resize", onResize, true);

    initTabbox();
};

var onResize = function () {
    adjustTabWidth(contentTabbox.tabs);
};

var onTocSelected = function (event) {
    var tree = event.target;
    var cellIndex = 1;
    var cellText = tree.view.getCellText(tree.currentIndex, tree.columns.getColumnAt(cellIndex));

    var browser = tree.browser;
    var url = CsScheme + cellText;
    d("onTocSelected", "index = " + tree.view.selection.currentIndex + ", url = " + url);
    browser.setAttribute("src", url);
};

var onTabSelect = function () {
    var currentPanel = contentTabbox.selectedPanel;
    setCommandStatus(currentPanel.type);
};

var onInputFilter = function (event) {
    var currentPanel = contentTabbox.selectedPanel;
    var book = currentPanel.book;
    var tree = currentPanel.treebox.index.tree;

    var filterText = event.target.value;
    rebuildIndexTree(tree, book.hhkData, filterText);
    d("onInputFilter", "filter text = " + filterText);
};

/*** Commands ***/

var openFile = function () {
    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);

    var strbundle = document.getElementById("bundle-main");
    var strSelectFile=strbundle.getString("selectChmFile");
    var strChmFile=strbundle.getString("chmFile");

    fp.init(window, strSelectFile, Ci.nsIFilePicker.modeOpen);
    fp.displayDirectory = Pref.getLastDir();
    fp.appendFilter(strChmFile, "*.chm;*.CHM");
    fp.appendFilters(Ci.nsIFilePicker.filterAll);

    // Popup open file dialog
    var res = fp.show();

    if (res == Ci.nsIFilePicker.returnOK) {
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

    contentTabbox.selectedIndex = index;
};

var closeTab = function () {
    if (contentTabbox.tabs.itemCount === 1)
        return;

    removeTab(getCurrentTab());
};

var goHome = function () {
    var panel = contentTabbox.selectedPanel;
    var tocTree = panel.treebox.toc.tree;
    panel.browser.setAttribute("src", CsScheme + panel.book.homepage);
    tocTree.view.selection.select(0);
    tocTree.treeBoxObject.ensureRowIsVisible(0);
};

var goBack = function () {
    contentTabbox.selectedPanel.browser.goBack();
};

var goForward = function () {
    contentTabbox.selectedPanel.browser.goForward();
};

var goPrevious = function () {
    var tocTree = contentTabbox.selectedPanel.treebox.toc.tree;
    var view = tocTree.view;
    var index = tocTree.currentIndex;

    if (index >= 1) {
        if (view.isContainer(index - 1) && !view.isContainerOpen(index - 1)) {
            view.toggleOpenState(index - 1);
            goPrevious();
        } else {
            view.selection.select(index - 1);
            tocTree.treeBoxObject.ensureRowIsVisible(index - 1);
        }
    }
};

var goNext = function () {
    var tocTree = contentTabbox.selectedPanel.treebox.toc.tree;
    var view = tocTree.view;
    var index = tocTree.currentIndex;

    if (index < view.rowCount - 1) {
        if (view.isContainer(index + 1) && !view.isContainerOpen(index + 1)) {
            view.toggleOpenState(index + 1);
            goNext();
        } else {
            view.selection.select(index + 1);
            tocTree.treeBoxObject.ensureRowIsVisible(index + 1);
        }
    }
};

var zoomIn = function () {
    var browser = contentTabbox.selectedPanel.browser;
    var zoom = browser.markupDocumentViewer.fullZoom;
    browser.markupDocumentViewer.fullZoom = zoom * 1.2;
};

var zoomOut = function () {
    var browser = contentTabbox.selectedPanel.browser;
    var zoom = browser.markupDocumentViewer.fullZoom;
    browser.markupDocumentViewer.fullZoom = zoom * 0.8;
};

var zoomReset = function () {
    contentTabbox.selectedPanel.browser.markupDocumentViewer.fullZoom = 1.0;
};

var togglePanel = function () {
    var button = document.getElementById("panel-btn");
    var splitter = contentTabbox.selectedPanel.splitter;

    if (button.checked) {
        splitter.setAttribute("state", "open");
        button.image = "chrome://chmsee/skin/show-pane.png";
    } else {
        splitter.setAttribute("state","collapsed");
        button.image = "chrome://chmsee/skin/hide-pane.png";
    }
};

/*** Other functions ***/

var initTabbox = function () {
    contentTabbox = document.getElementById("content-tabbox");
    contentTabbox.tabs.addEventListener("select", onTabSelect, true);
    appendTab(createWelcomeTab());
    contentTabbox.selectedIndex = 0;
};

var setCommandStatus = function(type) {
    if (type === "welcome") {
        document.getElementById("panel-btn").setAttribute("hidden", "true");
        document.getElementById("cmd_gohome").setAttribute("disabled", "true");
        document.getElementById("cmd_goback").setAttribute("disabled", "true");
        document.getElementById("cmd_goforward").setAttribute("disabled", "true");
        document.getElementById("cmd_goprevious").setAttribute("disabled", "true");
        document.getElementById("cmd_gonext").setAttribute("disabled", "true");
    } else {
        document.getElementById("panel-btn").setAttribute("hidden", "false");
        document.getElementById("cmd_gohome").removeAttribute("disabled");
        document.getElementById("cmd_goback").removeAttribute("disabled");
        document.getElementById("cmd_goforward").removeAttribute("disabled");
        document.getElementById("cmd_goprevious").removeAttribute("disabled");
        document.getElementById("cmd_gonext").removeAttribute("disabled");
    }
};

var createWelcomeTab = function () {
    var book = Book.getBookFromUrl("Welcome", "about:mozilla");

    var tab = document.createElement("tab");
    tab.setAttribute("align", "start");
    tab.setAttribute("crop", "end");
    tab.setAttribute("label", book.title);

    var panel = document.createElement("tabpanel");
    var browser = document.createElement("browser");
    browser.setAttribute("flex", 1);
    browser.setAttribute("type", "content");
    browser.setAttribute("src", book.homepage);
    panel.appendChild(browser);

    panel.type = "welcome";
    panel.book = book;
    panel.browser = browser;

    return {tab: tab, panel: panel};
};

var createBookTab = function (book) {
    var bookTab = document.createElement("tab");
    bookTab.setAttribute("label", book.title);
    bookTab.setAttribute("align", "start");
    bookTab.setAttribute("crop", "end");

    var bookPanel = document.createElement("tabpanel");
    bookPanel.setAttribute("orient", "vertical");
    bookPanel.setAttribute("flex", "1");

    var bookPanelBox = document.createElement("vbox");
    bookPanelBox.setAttribute("flex", "1");
    bookPanel.appendChild(bookPanelBox);

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
        var toc = createTreeTab("toc");
        treeTabs.appendChild(toc.tab);
        treePanels.appendChild(toc.panel);
        treeTabbox.toc = toc.treeBox;
    }

    if (book.hhkDS !== null) {
        var index = createTreeTab("index");
        treeTabs.appendChild(index.tab);
        treePanels.appendChild(index.panel);
        treeTabbox.index = index.treeBox;
    }

    bookContentBox.appendChild(treeTabbox);

    var splitter = document.createElement("splitter");
    splitter.setAttribute("collapse", "before");
    splitter.appendChild(document.createElement("grippy"));
    bookContentBox.appendChild(splitter);

    var browser = document.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("src", CsScheme + book.homepage);
    browser.setAttribute("flex", "1");
    bookContentBox.appendChild(browser);

    bookPanel.type = "book";
    bookPanel.book = book;
    bookPanel.treebox = treeTabbox;
    bookPanel.splitter = splitter;
    bookPanel.browser = browser;

    return {tab: bookTab, panel: bookPanel};
};

var createTreeTab = function (type) {
    var title = "TOC";
    var boxClass = "toc-treebox";

    if (type === "index") {
        title = "index";
        boxClass = "index-treebox";
    }

    var tab = document.createElement("tab");
    tab.setAttribute("label", title);

    var panel = document.createElement("tabpanel");
    var treeBox = document.createElement("vbox");
    treeBox.className = boxClass;
    treeBox.setAttribute("flex", 1);
    panel.appendChild(treeBox);

    return {tab: tab, panel: panel, treeBox: treeBox};
};

var appendTab = function (tab) {
    contentTabbox.tabs.appendChild(tab.tab);
    contentTabbox.tabpanels.appendChild(tab.panel);
    setCommandStatus(tab.panel.type);

    adjustTabWidth(contentTabbox.tabs);
};

var replaceTab = function (newTab, oldTab) {
    var currentIndex = contentTabbox.selectedIndex;

    contentTabbox.tabs.replaceChild(newTab.tab, oldTab.tab);
    contentTabbox.tabpanels.replaceChild(newTab.panel, oldTab.panel);

    if (currentIndex !== contentTabbox.selectedIndex)
        contentTabbox.selectedIndex = currentIndex;

    setCommandStatus(newTab.panel.type);
    adjustTabWidth(contentTabbox.tabs);
};

var removeTab = function (tab) {
    var tabs = contentTabbox.tabs;
    var tabpanels = contentTabbox.tabpanels;
    var currentIndex = contentTabbox.selectedIndex;

    tabs.removeChild(tab.tab);
    tabpanels.removeChild(tab.panel);
    adjustTabWidth(tabs);

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
        rebuildIndexTree(indexTree, book.hhkData, "");
        indexTree.browser = panel.browser;
    }

    if (treebox.tabs.itemCount === 1) {
        treebox.tabs.hidden = true;
    } else if (treebox.tabs.itemCount === 0) {
        treebox.hidden = true;
        splitter.hidden = true;
    }

    if (tocTree) {
        tocTree.view.selection.select(0);
        tocTree.focus();
    }

};

var rebuildIndexTree = function (tree, data, filterText) {
    var table = null;

    if (filterText === "") {
        table = data;
    } else {
        table = [];

        for (var i = 0; i < data.length; i++) {
            if ((data[i].name.toLowerCase()).indexOf(filterText) !== -1)
                table.push({name:data[i].name, local:data[i].local});
        }
    }

    table.sort(function (a, b) { return a.name > b.name; });
    tree.view = new TreeView(table);
};

var TreeView = function (table) {
    this.rowCount = table.length;
    this.getCellText = function(row, col) {
        if (col.index === 0)
            return table[row].name;
        else
            return table[row].local;
    };
    this.setTree = function(treebox) {
        this.treebox = treebox;
    };
    this.isContainer = function(row){ return false; };
    this.isSeparator = function(row){ return false; };
    this.isSorted = function(){ return false; };
    this.getLevel = function(row){ return 0; };
    this.getImageSrc = function(row,col){ return null; };
    this.getRowProperties = function(row,props){};
    this.getCellProperties = function(row,col,props){};
    this.getColumnProperties = function(colid,col,props){};
    this.cycleHeader = function(col, elem) {};
};

var getCurrentTab = function () {
    return { index: contentTabbox.selectedIndex,
             tab: contentTabbox.selectedTab,
             panel: contentTabbox.selectedPanel };
};

var adjustTabWidth = function (tabs) {
    var winWidth = window.outerWidth;
    var tabCount = tabs.itemCount;
    var tabWidth = Math.round(winWidth/tabCount);

    var totalWidth = 0;

    for (var i = 0; i < tabCount - 1; i++) {
        tabs.getItemAtIndex(i).width = tabWidth;
        totalWidth += tabWidth;
    }

    tabs.getItemAtIndex(tabCount-1).width = winWidth - totalWidth + 2*i;
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
