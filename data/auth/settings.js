var websock;

function listCONF(obj) {
	document.getElementById("adminpass").value = obj.auth_pass;
	document.getElementById("hstname").value = obj.wifi_hostname;
    document.getElementById("gain").value = obj.rfid_gain;
    document.getElementById("delay").value = obj.relay_time;
	document.getElementById("apssid").value = obj.ap_ssid;
	document.getElementById("appass").value = obj.ap_pass;
	document.getElementById("stassid").value = obj.sta_ssid;
    document.getElementById("stapass").value = obj.sta_pass;
	if (obj.wifimode === "1") {
		document.getElementById("wifimodeap").checked = true;
	}
	var dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(obj, null, 2));
	var dlAnchorElem = document.getElementById("downloadSet");
	dlAnchorElem.setAttribute("href", dataStr);
	dlAnchorElem.setAttribute("download", "esp-rfid-settings.json");
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
    document.getElementById("stassid").style.display = "none";
    var node = document.getElementById("ssid");
    node.style.display = "inline";
    while (node.hasChildNodes()) {
        node.removeChild(node.lastChild);
    }
}

function saveConf() {
	var adminpass = document.getElementById("adminpass").value;
	if (adminpass === null || adminpass === "") {
		alert("Administrator Password cannot be empty");
		return;
	}
    var ssid;
    if (document.getElementById("stassid").style.display === "none") {
        var b = document.getElementById("ssid");
        ssid = b.options[b.selectedIndex].value;
    } else {
        ssid = document.getElementById("stassid").value;
    }
	var wifimode = "0";
	if (document.getElementById("wifimodeap").checked) {
		wifimode = "1";
	}
    var datatosend = {};
    datatosend.command = "configfile";
	datatosend.auth_pass = adminpass;
	datatosend.wifi_hostname = document.getElementById("hstname").value;
    datatosend.rfid_gain = document.getElementById("gain").value;
    datatosend.relay_time = document.getElementById("delay").value;
	datatosend.ap_ssid = document.getElementById("apssid").value;
	datatosend.ap_pass = document.getElementById("appass").value;
	datatosend.sta_ssid = ssid;
    datatosend.sta_pass = document.getElementById("stapass").value;
	datatosend.wifimode = wifimode;
    websock.send(JSON.stringify(datatosend));
	location.reload();
}

function testRelay() {
    websock.send("{\"command\":\"testrelay\"}");
}

function backupuser() {
	websock.send("{\"command\":\"picclist\"}");
}

function backupset() {
	var dlAnchorElem = document.getElementById("downloadSet");
	dlAnchorElem.click();
}

function piccBackup(obj) {
	var dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(obj, null, 2));
	var dlAnchorElem = document.getElementById("downloadUser");
	dlAnchorElem.setAttribute("href", dataStr);
	dlAnchorElem.setAttribute("download", "esp-rfid-users.json");
	dlAnchorElem.click();
}

function restoreSet() {
	var input = document.getElementById("restoreSet");
	var reader = new FileReader();
	if ("files" in input) {
		if (input.files.length === 0) {
			alert("You did not select file to restore");
		} else {
			reader.onload = function() {
			var json;
			try {
				json = JSON.parse(reader.result);
			} catch (e) {
				alert("Not a valid backup file");
				return;
			}
			if (json.command === "configfile") {
				var x = confirm("File seems to be valid, do you wish to continue?");
				if (x) {
					websock.send(JSON.stringify(json));
					alert("Device now should reboot with new settings");
					location.reload();
				}
			}
			};
			reader.readAsText(input.files[0]);
		}
	}
}

function restoreUser() {
	var input = document.getElementById("restoreUser");
	var reader = new FileReader();
	if ("files" in input) {
		if (input.files.length === 0) {
			alert("You did not select any file to restore");
		} else {
			reader.onload = function() {
			var json;
			try {
				json = JSON.parse(reader.result);
			} catch (e) {
				alert("Not a valid backup file");
				return;
			}
			if (json.command === "picclist") {
				var x = confirm("File seems to be valid, do you wish to continue?");
				if (x) {
					document.getElementById("pbar").style.display = "block";
					var len = json.piccs.length;
					for (var i = 1; i <= len; i++) {
						var uid = json.piccs[i - 1].slice(3);
						var user = json.users[i - 1];
						var acc = json.access[i - 1];
						var timed = json.timedAcc[i - 1];
						var validdate = json.validDate[i - 1];
						restore1by1(i, uid, user, acc, timed, validdate, len);
					}
				}
			}
			};
			reader.readAsText(input.files[0]);
		}
	}
}

function restore1by1(n, uid, user, acc, timed, validdate, len) {
	setTimeout(function() {
		var datatosend = {};
		datatosend.command = "userfile";
		datatosend.uid = uid;
		datatosend.user = user;
		datatosend.haveAcc = acc;
		datatosend.timedAcc = timed;
		datatosend.validDate = validdate;
		websock.send(JSON.stringify(datatosend));
		var elem = document.getElementById("dynamic");
		var part = 100 / len;
		elem.style.width = part * n + "%";
		if (n === len) {
			document.getElementById("dynamic").innerHTML = "Completed";
		}
	}, 100 * n);
}

function refreshStats() {
	websock.send("{\"command\":\"status\"}");
	document.getElementById("status").style.display = "block";
	document.getElementById("refstat").innerHTML = "Refresh Statics";
}
  
function listStats(obj) {
	document.getElementById("chip").innerHTML = obj.chipid;
	document.getElementById("cpu").innerHTML = obj.cpu + " Mhz";
	document.getElementById("heap").innerHTML = obj.heap + " Bytes";
	document.getElementById("heap").style.width = (obj.heap * 100) / 81920 + "%";
	document.getElementById("flash").innerHTML = obj.availsize + " Bytes";
	document.getElementById("flash").style.width = (obj.availsize * 100) / 1044464 + "%";
	document.getElementById("ssidstat").innerHTML = obj.ssid;
	document.getElementById("ip").innerHTML = obj.ip;
	document.getElementById("gate").innerHTML = obj.gateway;
	document.getElementById("mask").innerHTML = obj.netmask;
	document.getElementById("dns").innerHTML = obj.dns;
	document.getElementById("mac").innerHTML = obj.mac;
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
		document.getElementById("loading-img").style.display = "none";
	};
	websock.onclose = function(evt) {};
	websock.onerror = function(evt) {
		console.log(evt);
	};
	websock.onmessage = function(evt) {
		var obj = JSON.parse(evt.data);
		if (obj.command === "ssidlist") {
			listSSID(obj);
		} else if (obj.command === "configfile") {
			listCONF(obj);
		} else if (obj.command === "picclist") {
			piccBackup(obj);
		} else if (obj.command === "status") {
			listStats(obj);
		}
	};
}