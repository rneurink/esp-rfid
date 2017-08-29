var websock;

function sortTable() {
	var table, rows, switching, i, x, y, shouldSwitch;
	table = document.getElementById("logtable");
	switching = true;
	/*Make a loop that will continue until
	no switching has been done:*/
	while (switching) {
		//start by saying: no switching is done:
		switching = false;
		rows = table.getElementsByTagName("TR");
		/*Loop through all table rows (except the
		first, which contains table headers):*/
		for (i = 1; i < (rows.length - 1); i++) {
			//start by saying there should be no switching:
			shouldSwitch = false;
			/*Get the two elements you want to compare,
			one from current row and one from the next:*/
			x = rows[i].getElementsByTagName("TD")[0];
			y = rows[i + 1].getElementsByTagName("TD")[0];
			if (x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {
				//if so, mark as a switch and break the loop:
				shouldSwitch = true;
				break;
			}
		}
		if (shouldSwitch) {
			/*If a switch has been marked, make the switch
			and mark that a switch has been done:*/
			rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);
			switching = true;
			//Each time a switch is done, increase this count by 1:
		}
	}
}

function addRowHandlers() {
	var table = document.getElementById("tablebody");
	var rows = table.getElementsByTagName("tr");
	for (var i = 0; i < rows.length; i++) {
		var currentRow = table.rows[i];
		var createClickHandler =
			function(row) {
				return function() {
					
				};
			};
		currentRow.onclick = createClickHandler(currentRow);
	}
}

function listdates(obj) {
	var ref = document.getElementById("datelist");
	var i;
	for (i = ref.options.length - 1; i >= 0; i--) {
		ref.remove(i);
	}
	for (i = 0; i < obj.date.length; i++) {
		var formatteddate = obj.date[i].substr(0,4) + "-" + obj.date[i].substr(4,2) + "-" + obj.date[i].substr(6,2);
		var ref2 = document.createElement("option");
		ref2.text = formatteddate;
		ref2.value = obj.date[i];
		ref.add(ref2);
	}
	ref.selectedIndex = i - 1;
	getlog();
}

function getlog() {
	var datatosend = {};
	datatosend.command = "loglist";
	datatosend.msg = document.getElementById("datelist").value;
	websock.send(JSON.stringify(datatosend));
}

function removeLog() {
	var ref = document.getElementById("datelist");
	var x = confirm("This will remove all log entries from: " + ref.value.substr(0,4) + "-" + ref.value.substr(4,2) + "-" + ref.value.substr(6,2) + ". Are you sure?");
	if (x) {
		var datatosend = {};
		datatosend.command = "remlog";
		datatosend.msg = ref.value;
		websock.send(JSON.stringify(datatosend));
		location.reload();
	}
}

function listknownLogs(obj) {
	var table = document.getElementById("logtable").getElementsByTagName("tbody")[0];
	while(table.rows.length > 0) {
		table.deleteRow(0);
	}
	for (var i = 0; i < obj.time.length; i++) {
		var row = table.insertRow(table.rows[0]);
		row.className = "success";
		row.id = obj.time[i];
		var cell1 = row.insertCell(0);
		cell1.innerHTML = obj.time[i];
		var cell2 = row.insertCell(1);
		cell2.innerHTML = obj.uid[i].toUpperCase();
		var cell3 = row.insertCell(2);
		cell3.innerHTML = obj.username[i];
		var cell4 = row.insertCell(3);
		if (obj.granted[i] == 1) {
			row.className = "success";
			cell4.innerHTML = "Granted";
		} else {
			row.className = "warning";
			cell4.innerHTML = "Denied";
		}
	}
}

function start() {
	websock = new WebSocket("ws://" + window.location.hostname + "/ws");
	websock.onopen = function(evt) {
		websock.send("{\"command\":\"datelist\"}");
	};
	websock.onclose = function(evt) {
		
	};
	websock.onerror = function(evt) {
		console.log(evt);
	};
	websock.onmessage = function(evt) {
		var obj = JSON.parse(evt.data);
		if (obj.command === "loglist") {
			listknownLogs(obj);
			sortTable();
			addRowHandlers();
		} else if (obj.command === "datelist") {
			var node = document.getElementById("tablebody");
			while (node.hasChildNodes()) {
				node.removeChild(node.lastChild);
			}
			document.getElementById("loading-img").style.display = "none";
			listdates(obj);
		}
	};
}