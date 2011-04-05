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

function ContentHandler(parseInfo) {
    this.ds = parseInfo.ds;
    this.folder = parseInfo.folder;
    this.isItem = false;
    this.name = "";
    this.local = "";
    this.containers = [];
    this.lastRes = null;
}

ContentHandler.prototype = {
    startElement: function(tag, attrs) {
        d("Handler::startElement", "tag = " + tag + ", attrs length = " + attrs.length);

        if (tag.ncmp("ul")) {
            var resource = null;

            if (this.containers.length == 0) {
                resource = rdfService.GetResource("urn:chmsee:root");
            } else {
                resource = this.lastRes;
            }
            var container = rdfContainerUtils.MakeSeq(this.ds, resource);
            this.containers.push(container);
        } else if (tag.ncmp("object")) {
            if (attrs.length > 0 && attrs[0].name.toLowerCase() == "type" && attrs[0].value.toLowerCase() == "text/sitemap") {
                this.isItem = true;
                d("Handler::startElement", "attrs[0].name = " + attrs[0].name + ", attrs[0].value = " + attrs[0].value + ", item = " + this.isItem);
            }
        } else if (tag.ncmp("param")) {
            if (attrs.length > 0 && attrs[0].value.toLowerCase() == "name")
                this.name = attrs[1].value;
            if (attrs.length > 0 && attrs[0].value.toLowerCase() == "local")
                this.local = attrs[1].value;
        }
    },

    endElement: function(tag) {
        d("Handler::endElement", "name = " + tag);
        if (tag.ncmp("ul")) {
            if (this.containers.length > 0) {
                this.containers.pop();
            }
        } else if (tag.toLowerCase() == "object" && this.isItem) {
            var res = rdfService.GetAnonymousResource();

            var predicate = rdfService.GetResource("urn:chmsee:rdf#name");
            var object = rdfService.GetLiteral(this.name);
            this.ds.Assert(res, predicate, object, true);

            predicate = rdfService.GetResource("urn:chmsee:rdf#local");
            object = rdfService.GetLiteral(this.folder + this.local);
            this.ds.Assert(res, predicate, object, true);

            this.containers[this.containers.length - 1].AppendElement(res);

            this.lastRes = res;
            this.isItem = false;
        }
    },

    characters: function(str) {
        d("Handler::characters", str);
    },

    comment: function(str) {
        d("Handler::comment", str);
    },
};

function generateRdf(file, bookfolder, datasource)
{
    var data = "";
    var fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(Ci.nsIFileInputStream);
    var cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].createInstance(Ci.nsIConverterInputStream);

    fstream.init(file, -1, 0, 0);
    cstream.init(fstream, "UTF-8", 0, 0);

    let (str = {}) {
        let read = 0;
        do {
            read = cstream.readString(0xffffffff, str);
            data += str.value;
        } while (read != 0);
    }
    cstream.close();

    var tmpNameSpace = {};
    var sl = Cc["@mozilla.org/moz/jssubscript-loader;1"].createInstance(Ci.mozIJSSubScriptLoader);
    sl.loadSubScript("chrome://chmsee/content/simpleparser.js", tmpNameSpace);

    var parser = new tmpNameSpace.SimpleHtmlParser();
    var pinfo = {folder: bookfolder, ds: datasource};
    parser.parse(data, new ContentHandler(pinfo));
}
