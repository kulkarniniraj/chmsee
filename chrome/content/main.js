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

const dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
const rdfService = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
const rdfContainerUtils = Cc["@mozilla.org/rdf/container-utils;1"].createInstance(Ci.nsIRDFContainerUtils);

var bookshelf;

const CsDebug = true;

function d(f, s) {
    if (CsDebug)
        dump(f + " >>> " + s + "\n");
}

function Book() {
    this.md5 = "";
    this.folder = "";
    this.homepage = "";
    this.url = "";
    this.title = "";
    this.hhc = null;
    this.hhcDS = null;
    this.hhk = null;
    this.hhkDS = null;

    this.initWithUrl = function(title, url) {
        this.title = title;
        this.homepage = url;
        this.url = url;
    };

    this.initWithFile = function(file) {
        this.md5 = md5Hash(file);
        d("Book::initWithFile", "chm md5 = " + this.md5);

        if (this.initWithMd5(this.md5) == true) {
            d("Book::initWithFile", "get existed bookinfo.");
            return true;
        }

        try {
            var chmobj = Cc["@chmsee/cschm;1"].createInstance();
            chmobj.QueryInterface(Ci.csIChm);
            chmobj.openChm(file, this.folder);

            this.homepage = this.folder + chmobj.homepage;
            d("Book::initWithFile", "chm homepage = " + this.homepage);
            this.url = "chmsee://" + this.homepage;

            this.title = chmobj.bookname;
            d("Book::initWithFile", "lcid = " + chmobj.lcid);

            if (chmobj.hhc != null) {
                d("Book::initWithFile", "hhc = " + chmobj.hhc);
                this.hhc = this.folder + chmobj.hhc;
                this.hhcDS = getRdfDatasource("hhc", this.folder, this.hhc);
            }

            if (chmobj.hhk != null) {
                d("Book::initWithFile", "hhk = " + chmobj.hhk);
                this.hhk = this.folder + chmobj.hhk;
                this.hhkDS = getRdfDatasource("hhk", this.folder, this.hhk);
            }

            // Save loaded book info
            var infoFile = this.folder + "/chmsee_bookinfo.rdf";

            var infoDS = rdfService.GetDataSourceBlocking("file://" + infoFile);
            var res = rdfService.GetResource("urn:chmsee:bookinfo");

            var predicate = rdfService.GetResource("urn:chmsee:rdf#homepage");
            var object = rdfService.GetLiteral(this.homepage);
            infoDS.Assert(res, predicate, object, true);

            predicate = rdfService.GetResource("urn:chmsee:rdf#title");
            object = rdfService.GetLiteral(this.title);
            infoDS.Assert(res, predicate, object, true);

            if (this.hhc != null) {
                predicate = rdfService.GetResource("urn:chmsee:rdf#hhc");
                object = rdfService.GetLiteral(this.hhc);
                infoDS.Assert(res, predicate, object, true);
            }

            if (this.hhk != null) {
                predicate = rdfService.GetResource("urn:chmsee:rdf#hhk");
                object = rdfService.GetLiteral(this.hhk);
                infoDS.Assert(res, predicate, object, true);
            }

            infoDS.QueryInterface(Ci.nsIRDFRemoteDataSource);
            infoDS.Flush();

        } catch(e) {
            alert(e);
            return false;
        }

        return true;
    };

    this.initWithMd5 = function(md5) {
        this.md5 = md5;
        this.folder = bookshelf + this.md5 + "/";

        var infoFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
        infoFile.initWithPath(this.folder + "/chmsee_bookinfo.rdf");

        if (infoFile.exists() == false) {
            d("Book::initWithMd5", "book info isn't exists");
            return false;
        }

        d("Book::initWithMd5", "book info = " + infoFile.path);

        var infoDS = rdfService.GetDataSourceBlocking("file://" + infoFile.path);
        var res = rdfService.GetResource("urn:chmsee:bookinfo");

        var target = infoDS.GetTarget(res, rdfService.GetResource("urn:chmsee:rdf#homepage"), true);

        if (target instanceof Ci.nsIRDFLiteral) {
            this.homepage = target.Value;
        }
        d("Book::initWithMd5", "homepage = " + this.homepage);
        this.url = "chmsee://" + this.homepage;

        target = infoDS.GetTarget(res, rdfService.GetResource("urn:chmsee:rdf#title"), true);
        if (target instanceof Ci.nsIRDFLiteral) {
            this.title = target.Value;
        }
        d("Book::initWithMd5", "title = " + this.title);

        target = infoDS.GetTarget(res, rdfService.GetResource("urn:chmsee:rdf#hhc"), true);
        if (target instanceof Ci.nsIRDFLiteral) {
            this.hhc = target.Value;
        }

        d("Book::initWithMd5", "hhc = " + this.hhc);
        if (this.hhc != null) {
            var rdf = this.hhc.slice(0, this.hhc.lastIndexOf(".hhc")) + "_hhc.rdf";
            d("Book::initWithMd5", "rdf path = " + rdf);

            this.hhcDS = rdfService.GetDataSourceBlocking("file://" + rdf);
        }

        d("Book::initWithMd5", "hhk = " + this.hhk);
        if (this.hhk != null) {
            var rdf = this.hhk.slice(0, this.hhk.lastIndexOf(".hhk")) + "_hhk.rdf";
            d("Book::initWithMd5", "rdf path = " + rdf);

            this.hhkDS = rdfService.GetDataSourceBlocking("file://" + rdf);
        }

        return true;
    };
}

function onWindowLoad() {
    d("windowInit", "starter");

    bookshelf = dirService.get("Home", Ci.nsIFile).path + "/.chmsee/bookshelf/";

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
    browser.setAttribute("src", url);
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

    var book = new Book();
    book.initWithUrl("Starter", "about:mozilla");
    updateTab(book);
}

function createTab() {
    var tabbox = document.getElementById("content-tabbox");
    var tab = document.createElement("tab");
    tabbox.tabs.appendChild(tab);

    var panel = document.createElement("tabpanel");
    panel.setAttribute("class", "book-tabpanel");
    tabbox.tabpanels.appendChild(panel);

    return tabbox.tabs.itemCount - 1;
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

function updateTab(book) {
    var tabbox = document.getElementById("content-tabbox");
    var tab = tabbox.selectedTab;
    tab.setAttribute("label", book.title);

    var panel = tabbox.selectedPanel;
    panel.setAttribute("url", book.url);
    panel.book = book;

    if (book.hhcDS == null) {
        panel.setAttribute("splitter", "collapsed");
    } else {
        panel.setTree(book.hhcDS);
        panel.setAttribute("splitter", "open");
    }
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

function goHome() {
    document.getElementById("browser").goHome();
}

function md5Hash(file) {
    var istream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);

    istream.init(file, 0x01, 0444, 0);

    var ch = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
    ch.init(ch.MD5);
    ch.updateFromStream(istream, 0xffffffff);
    var hash = ch.finish(false);

    // return the two-digit hexadecimal code for a byte
    function toHexString(charCode) {
        return ("0" + charCode.toString(16)).slice(-2);
    }

    var s = [];
    for (var i = 0; i < hash.length; i++)
        s.push(toHexString(hash.charCodeAt(i)));
    return s.join("");
}

String.prototype.ncmp = function(str) {
    return this.toLowerCase() == str.toLowerCase() ? true : false;
};

function getRdfDatasource(type, bookfolder, path) {
    var rdfPath;

    if (type == "hhc")
        rdfPath = path.slice(0, path.lastIndexOf(".hhc")) + "_hhc.rdf";
    else if (type == "hhk")
        rdfPath = path.slice(0, path.lastIndexOf(".hhk")) + "_hhk.rdf";
    else
        return null;

    d("getRdfDatasource", "rdfPath = " + rdfPath);

    var datasource = rdfService.GetDataSourceBlocking("file://" + rdfPath);

    var tmpNameSpace = {};
    var sl = Cc["@mozilla.org/moz/jssubscript-loader;1"].createInstance(Ci.mozIJSSubScriptLoader);
    sl.loadSubScript("chrome://chmsee/content/rdfUtils.js", tmpNameSpace);

    if (type == "hhc")
        treeType = true;
    else
        treeType = false;

    var sourceFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    sourceFile.initWithPath(path);

    tmpNameSpace.generateRdf(treeType, sourceFile, bookfolder, datasource);

    datasource.QueryInterface(Ci.nsIRDFRemoteDataSource);
    datasource.Flush();

    return datasource;
}

function openFile() {
    // Read last opened directory
    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
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

        var book = new Book();
        book.initWithFile(fp.file);

        updateTab(book);
    }
}

// Call DOM inspector for debugging
function startDOMi()
{
    // Load the Window DataSource so that browser windows opened subsequent to DOM
    // Inspector show up in the DOM Inspector's window list.
    var windowDS = Cc["@mozilla.org/rdf/datasource;1?name=window-mediator"].getService(Ci.nsIWindowDataSource);
    var tmpNameSpace = {};
    var sl = Cc["@mozilla.org/moz/jssubscript-loader;1"].createInstance(Ci.mozIJSSubScriptLoader);
    sl.loadSubScript("chrome://inspector/content/hooks.js", tmpNameSpace);
    tmpNameSpace.inspectDOMDocument(document);
}
