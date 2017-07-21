var websock;

function listCONF(obj) {
	document.getElementById("adminpass").value = obj.auth_pass;
	document.getElementById("hstname").value = obj.wifi_hostname;
    document.getElementById("gain").value = obj.rfid_gain;
    document.getElementById("delay").value = obj.relay_time;
	document.getElementById("apssid").value = obj.ap_ssid;
	document.getElementById("appass").value = obj.ap_pass;
	document.getElementById("inputtohide").value = obj.sta_ssid;
    document.getElementById("stapass").value = obj.sta_pass;
}

function listSSID(obj) {
    var select = document.getElementById("ssid");
    for (var i = 0; i < obj.ssid.length; i++) {
        var opt = document.createElement("option");
        opt.value = obj.ssid[i];
        opt.innerHTML = obj.ssid[i];
        select.appendChild(opt);
    }
    document.getElementById("scanb").innerHTML = "Re-Scan";
}

function scanWifi() {
    websock.send("{\"command\":\"scan\"}");
    document.getElementById("scanb").innerHTML = "...";
    document.getElementById("inputtohide").style.display = "none";
    var node = document.getElementById("ssid");
    node.style.display = "inline";
    while (node.hasChildNodes()) {
        node.removeChild(node.lastChild);
    }
}

function saveConf() {
    var ssid;
    if (document.getElementById("inputtohide").style.display === "none") {
        var b = document.getElementById("ssid");
        ssid = b.options[b.selectedIndex].value;
    } else {
        ssid = document.getElementById("inputtohide").value;
    }
    var datatosend = {};
    datatosend.command = "configfile";
	datatosend.auth_pass = document.getElementById("adminpass").value;
	datatosend.wifi_hostname = document.getElementById("hstname").value;
    datatosend.rfid_gain = document.getElementById("gain").value;
    datatosend.relay_time = document.getElementById("delay").value;
	datatosend.ap_ssid = document.getElementById("apssid").value;
	datatosend.ap_pass = document.getElementById("appass").value;
	datatosend.sta_ssid = ssid;
    datatosend.sta_pass = document.getElementById("stapass").value;
    websock.send(JSON.stringify(datatosend));
	location.reload();
}

function testRelay() {
    websock.send("{\"command\":\"testrelay\"}");
}

function clearLog() {
	websock.send("{\command\":\"clearlog\"}");
	var x = confirm("This will remove all logs. Are you sure?");
    if (x) {
        //var jsontosend = "{\"uid\":\"" + e.id + "\",\"command\":\"remove\"}";
        //websock.send(jsontosend);
    }
}

function start() {
    websock = new WebSocket("ws://" + window.location.hostname + "/ws");
    websock.onopen = function(evt) {
        websock.send("{\"command\":\"getconf\"}");
    };
    websock.onclose = function(evt) {
		setTimeout(function(){start()}, 5000);
    };
    websock.onerror = function(evt) {
        console.log(evt);
    };
    websock.onmessage = function(evt) {
        var obj = JSON.parse(evt.data);
        if (obj.command === "ssidlist") {
            listSSID(obj);
        } else if (obj.command === "configfile") {
            listCONF(obj);
            document.getElementById("loading-img").style.display = "none";
        }
    };
}