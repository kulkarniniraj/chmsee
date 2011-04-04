const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const nsIProtocolHandler    = Ci.nsIProtocolHandler;
const nsIURI                = Ci.nsIURI;
const nsIIOService          = Ci.nsIIOService;
const nsIPrefService        = Ci.nsIPrefService;
const nsIWindowWatcher      = Ci.nsIWindowWatcher;
const nsIChannel            = Ci.nsIChannel;
const nsIContentPolicy      = Ci.nsIContentPolicy;

const SCHEME = "chmsee";
const PROTOCOL_CID = Components.ID("e75fd986-51d4-11e0-938b-00241d8cf371");

const CsDebug = true;

function d(f, s) {
    if (CsDebug)
        dump(f + " >>> " + s + "\n");
}

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function ChmseeProtocolHandler(scheme) {
    this.scheme = scheme;
}

ChmseeProtocolHandler.prototype = {
    defaultPort: -1,
    protocolFlags: nsIProtocolHandler.URI_NORELATIVE |
                   nsIProtocolHandler.URI_NOAUTH,

    allowPort: function(aPort, aScheme) {
        return false;
    },

    newURI: function(aSpec, aCharset, aBaseURI) {
        d("newURI", "aSpec = " + aSpec + ", aCharset = " + aCharset + ", aBaseURI = " + aBaseURI.spec);

        var uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(nsIURI);

        if (aSpec.indexOf("chmsee://") == 0) {
            uri.spec = aSpec;
        } else {
            var base = aBaseURI.spec;
            var pos = base.lastIndexOf("/");
            var dir = base.substring(0, pos + 1);
            d("newURI", "dir = " + dir);

            if (pos > 0) {
                uri.spec = dir + aSpec;
            }
        }

        return uri;
    },

    newChannel: function(aURI) {
        d("newChannel", "aURI.spec = " + aURI.spec);

        var filepath = "file://" + aURI.spec.substring(9);
        d("newChannel", "filepath = " + filepath);

        var iOService = Cc["@mozilla.org/network/io-service;1"].getService(nsIIOService);
        var channel = iOService.newChannel(filepath, null, null);
        return channel;
    },
};

function ChmseeProtocolHandlerFactory(scheme) {
    this.scheme = scheme;
}

ChmseeProtocolHandlerFactory.prototype = {
    createInstance: function(outer, iid) {
        if (outer != null)
            throw Cr.NS_ERROR_NO_AGGREGATION;

        if (!iid.equals(nsIProtocolHandler))
            throw Cr.NS_ERROR_NO_INTERFACE;

        return new ChmseeProtocolHandler(this.scheme);
    },
};

function Chmsee () {
    this.wrappedJSObject = this;
}

Chmsee.prototype = {
    classID: PROTOCOL_CID,
    _xpcom_factory: new ChmseeProtocolHandlerFactory(SCHEME),
    QueryInterface: XPCOMUtils.generateQI([nsIProtocolHandler]),
};

var csComponents = [Chmsee];

const NSGetFactory = XPCOMUtils.generateNSGetFactory(csComponents);