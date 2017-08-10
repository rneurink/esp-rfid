var websock;

function sortTable() {
	var table, rows, switching, i, x, y, shouldSwitch;
	table = document.getElementById("knowntable");
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
			x = rows[i].getElementsByTagName("TD")[1];
			y = rows[i + 1].getElementsByTagName("TD")[1];
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

function fadeOutIn(elem, speed) {
	if (!elem.style.opacity) {
		elem.style.opacity = 1;
	} // end if
	var outInterval = setInterval(function() {
		elem.style.opacity -= 0.02;
		if (elem.style.opacity <= 0) {
			clearInterval(outInterval);
			var inInterval = setInterval(function() {
				elem.style.opacity = Number(elem.style.opacity) + 0.02;
				if (elem.style.opacity >= 1) {
					clearInterval(inInterval);
				}
			}, speed / 50);
		} // end if
	}, speed / 50);
}

function listSCAN(obj) {
	var isKnown = obj.known;
	var uid = obj.uid;
	var uidUP = obj.uid.toUpperCase();
	document.getElementById("uidinp").value = uidUP;
	document.getElementById("typeinp").value = obj.type;
	document.getElementById("username").value = obj.user;
	document.getElementById("access").value = obj.acctype;
	document.getElementById("validdate").value = obj.validDate;
	var ref = document.getElementById("buttonnormal");
	var ref2 = document.getElementById("rembuttonnormal");
	if (obj.access == 2) {
		ref.style.display = "none";
		ref2.style.display = "none";
		ref = document.getElementById("buttontimetable");
		ref2 = document.getElementById("rembuttontimetable");
		document.getElementById("timetablecol").style.display = "block";
		parseTimeData(obj.timedAcc);
	}
	else {
		document.getElementById("timetablecol").style.display = "none";
		clearTimeData();
	}
	ref.style.display = "block";
	if (isKnown === 1) {
		ref.dep = uid;
		ref.className = "btn btn-warning btn-sm";
		ref.onclick = function() {
			update(this);
		};
		ref.textContent = "Update";
		ref2.style.display = "block";
	} else {
		ref.dep = uid;
		ref.className = "btn btn-success btn-sm";
		ref.onclick = function() {
			update(this);
		};
		ref.textContent = "Add";
		ref2.style.display = "none";
	}
	fadeOutIn(document.getElementById("fade"), 250);
	fadeOutIn(document.getElementById("timetable"), 250);
}

function del() {
	var uid = document.getElementById("uidinp").value.toLowerCase();
	var username = document.getElementById("username").value;
	var x = confirm("This will remove " + uid.toUpperCase() + " : " + username + " from database. Are you sure?");
	if (x) {
		var jsontosend = "{\"uid\":\"" + uid + "\",\"command\":\"remove\"}";
		websock.send(jsontosend);
		websock.send("{\"command\":\"picclist\"}");
	}
}

function update(e) {
	var datatosend = {};
	datatosend.command = "userfile";
	datatosend.uid = document.getElementById("uidinp").value.toLowerCase();
	datatosend.user = document.getElementById("username").value;
	datatosend.accType = document.getElementById("access").value;
	datatosend.timedAcc = getTimeString();
	if (!checkDateFormat) return;
	datatosend.validDate = document.getElementById("validdate").value;
	websock.send(JSON.stringify(datatosend));
	websock.send("{\"command\":\"picclist\"}");
}

function addRowHandlers() {
	var table = document.getElementById("tablebody");
	var rows = table.getElementsByTagName("tr");
	for (var i = 0; i < rows.length; i++) {
		var currentRow = table.rows[i];
		var createClickHandler =
			function(row) {
				return function() {
					document.getElementById("uidinp").value = row.getElementsByTagName("td")[0].innerHTML;
					document.getElementById("username").value = row.getElementsByTagName("td")[1].innerHTML;
					document.getElementById("typeinp").value = "";
					if (row.getElementsByTagName("td")[2].innerHTML == "Infinite") {
						document.getElementById("access").value = "1";
						document.getElementById("timetablecol").style.display = "none";
					} else if (row.getElementsByTagName("td")[2].innerHTML == "No access"){
						document.getElementById("access").value = "0";
						document.getElementById("timetablecol").style.display = "none";
					} else if (row.getElementsByTagName("td")[2].innerHTML == "Timed") {
						document.getElementById("access").value = "2";
						document.getElementById("timetablecol").style.display = "block";
						if (document.getElementById("buttonnormal").style.display == "block") document.getElementById("buttonnormal").style.display = "none";
						if (document.getElementById("rembuttonnormal").style.display == "block") document.getElementById("rembuttonnormal").style.display = "none";
						parseTimeData(row.getElementsByTagName("td")[3].innerHTML);
					}
					document.getElementById("validdate").value = row.getElementsByTagName("td")[4].innerHTML;
					var ref = document.getElementById("buttonnormal");
					var ref2 = document.getElementById("rembuttonnormal");
					if (document.getElementById("access").value == 2) {
						ref = document.getElementById("buttontimetable");
						ref2 = document.getElementById("rembuttontimetable");
					}
					ref.style.display = "block";
					ref2.style.display = "block";
					ref.dep = document.getElementById("uidinp").value.toLowerCase();
					ref.className = "btn btn-warning btn-sm";
					ref.onclick = function() {
						update(this);
					};
					ref.textContent = "Update";
				};
			};
		currentRow.onclick = createClickHandler(currentRow);
	}
}

function listknownPICC(obj) {
	var table = document.getElementById("knowntable").getElementsByTagName("tbody")[0];
	for (var i = 0; i < obj.piccs.length; i++) {
		var x = obj.piccs[i];
		x = x.substring(3);
		var upper = x.toUpperCase();
		var row = table.insertRow(table.rows[0]);
		row.className = "success";
		row.id = x;
		var cell1 = row.insertCell(0);
		cell1.innerHTML = upper;
		var cell2 = row.insertCell(1);
		cell2.innerHTML = obj.users[i];
		var cell3 = row.insertCell(2);
		switch(obj.accType[i]){
			case 0:
				cell3.innerHTML = "No access";
				break;
			case 1:
				cell3.innerHTML = "Infinite";
				break;
			case 2:
				cell3.innerHTML = "Timed";
				break;
		}
		var cell4 = row.insertCell(3);
		cell4.innerHTML = obj.timedAcc[i];
		cell4.style.display = "none";
		var cell5 = row.insertCell(4);
		cell5.innerHTML = obj.validDate[i];
		cell5.style.display = "none";
		if (obj.accType[i] == 1) {
			row.className = "success";
		} else if (obj.accType[i] == 0) {
			row.className = "warning";
		} else if (obj.accType[i] == 2) {
			row.className = "info";
		}
	}
}

function showTimeMenu() {
	var value = document.getElementById("access").value;
	if (value == 2) {
		var ref = document.getElementById("timetablecol");
		ref.style.display = "block";
		if (document.getElementById("buttonnormal").style.display == "block") {
			var but = document.getElementById("buttontimetable");
			var butref = document.getElementById("buttonnormal");
			if (document.getElementById("rembuttonnormal").style.display == "block") {
				document.getElementById("rembuttonnormal").style.display = "none";
				document.getElementById("rembuttontimetable").style.display = "block";
			}
			but.style.display = "block";
			but.dep = butref.dep;
			but.className = butref.className;
			but.onclick = function() {
				update(this);
			};
			but.textContent = butref.textContent;
			butref.style.display = "none";
		}
	}
	else {
		var ref = document.getElementById("timetablecol");
		ref.style.display = "none";
		if (document.getElementById("buttontimetable").style.display == "block") {
			var but = document.getElementById("buttonnormal");
			var butref = document.getElementById("buttontimetable");
			if (document.getElementById("rembuttontimetable").style.display == "block") {
				document.getElementById("rembuttonnormal").style.display = "block";
				document.getElementById("rembuttontimetable").style.display = "none";
			}
			but.style.display = "block";
			but.dep = butref.dep;
			but.className = butref.className;
			but.onclick = function() {
				update(this);
			};
			but.textContent = butref.textContent;
			butref.style.display = "none";
		}
	}
}

function checkTimeFormat() {
	var timeInputs = ["fromSunday","untillSunday","fromMonday","untillMonday","fromTuesday","untillTuesday","fromWednesday","untillWednesday","fromThursday","untillThursday","fromFriday","untillFriday","fromSaturday","untillSaturday"];
	for (var i = 0; i < timeInputs.length; i++)
	{
		var ref = document.getElementById(timeInputs[i]);
		if (ref.value === null || ref.value === "") continue;
		if (i % 2 == 0) {
			var ref2 = document.getElementById(timeInputs[i+1]);
			if (!(ref2.value === null || ref2.value === "")) {
				var refval = ref.value.split(":");
				var ref2val= ref2.value.split(":");
				if (refval[0] > ref2val[0] || (refval[0] == ref2val[0] && refval[1] > ref2val[1])) {
					var tempval = ref.value;
					ref.value = ref2.value;
					ref2.value = tempval;
				}
			}
		}
		var result = false, m;
		var re = /^\s*([01]?\d|2[0-3]):?([0-5]\d)\s*$/;
		if ((m = ref.value.match(re))) {
			result = (m[1].length === 2 ? "" : "0") + m[1] + ":" + m[2];
		}
		if (!result) {
			alert("Invalid time format: " + document.getElementById(timeInputs[i]).value);
			return false;
		}
	}
	return true;
}

function checkDateFormat() { 
	var ref = document.getElementById("validdate");
	var refval = ref.value;
	if (refval === null || refval === "") return true;
	if (!validatedate()) return false;
	
	if (refval.charAt(2) == "-" && refval.length != 10) {
		var replacement = "0" + refval.substr(3,6);
		refval = refval.slice(0,-6);
		refval += replacement;
	} else if (refval.charAt(1) == "-" && refval.length != 10) {
		var replacement = "0" + refval;
		refval = replacement;
	}
	ref.value = refval;
	
	if (refval.length < 10) checkDateFormat();
	return true;
}

function validatedate() {  
	var dateformat = /^\d{4}[\-](0?[1-9]|1[012])[\-](0?[1-9]|[12][0-9]|3[01])$/;  
	var ref = document.getElementById("validdate");
	if(ref.value.match(dateformat)) {  
		ref.focus();  
		var pdate = ref.value.split('-');  
		
		var dd = parseInt(pdate[2]);  
		var mm  = parseInt(pdate[1]);  
		var yy = parseInt(pdate[0]);  
		// Create list of days of a month [assume there is no leap year by default]  
		var ListofDays = [31,28,31,30,31,30,31,31,30,31,30,31];  
		if (mm==1 || mm>2) {  
			if (dd>ListofDays[mm-1]) {  
				alert('Invalid date format!');  
				return false;  
			}  
		}  
		if (mm==2) {  
			var lyear = false;  
			if ( (!(yy % 4) && yy % 100) || !(yy % 400)) {  
				lyear = true;  
			}  
			if ((lyear==false) && (dd>=29)) {  
				alert('Invalid date format!');  
				return false;  
			}  
			if ((lyear==true) && (dd>29)) {  
				alert('Invalid date format!');  
				return false;  
			}  
		}  
	}  
	else {  
		alert("Invalid date format!");  
		ref.focus();  
		return false;  
	}
	return true;
}  

function clearTimeData() {
	var timeInputs = ["checkSunday","fromSunday","untillSunday","checkMonday","fromMonday","untillMonday","checkTuesday","fromTuesday","untillTuesday","checkWednesday","fromWednesday","untillWednesday","checkThursday","fromThursday","untillThursday","checkFriday","fromFriday","untillFriday","checkSaturday","fromSaturday","untillSaturday"];
	for (var i = 0; i < timeInputs.length; i += 3) {
		document.getElementById(timeInputs[i]).checked = false;
		document.getElementById(timeInputs[i+1]).value = "";
		document.getElementById(timeInputs[i+2]).value = "";
	}
}

function parseTimeData(str)
{
	var timearr = str.split(" ");
	clearTimeData();
	for (var i = 0; i < timearr.length; i ++) {
		switch(parseInt(timearr[i].charAt(0))) {
			case 0:
				document.getElementById("checkSunday").checked = true;
				document.getElementById("fromSunday").value = timearr[i].substr(2,5);
				document.getElementById("untillSunday").value = timearr[i].substr(8,5);
				break;
			case 1:
				document.getElementById("checkMonday").checked = true;
				document.getElementById("fromMonday").value = timearr[i].substr(2,5);
				document.getElementById("untillMonday").value = timearr[i].substr(8,5);
				break;
			case 2:
				document.getElementById("checkTuesday").checked = true;
				document.getElementById("fromTuesday").value = timearr[i].substr(2,5);
				document.getElementById("untillTuesday").value = timearr[i].substr(8,5);
				break;
			case 3:
				document.getElementById("checkWednesday").checked = true;
				document.getElementById("fromWednesday").value = timearr[i].substr(2,5);
				document.getElementById("untillWednesday").value = timearr[i].substr(8,5);
				break;
			case 4:
				document.getElementById("checkThursday").checked = true;
				document.getElementById("fromThursday").value = timearr[i].substr(2,5);
				document.getElementById("untillThursday").value = timearr[i].substr(8,5);
				break;
			case 5:
				document.getElementById("checkFriday").checked = true;
				document.getElementById("fromFriday").value = timearr[i].substr(2,5);
				document.getElementById("untillFriday").value = timearr[i].substr(8,5);
				break;
			case 6:
				document.getElementById("checkSaturday").checked = true;
				document.getElementById("fromSaturday").value = timearr[i].substr(2,5);
				document.getElementById("untillSaturday").value = timearr[i].substr(8,5);
				break;
			default:
				break;
		}
	}
}

function getTimeString() {
	if (!checkTimeFormat()) return;
	var timeInputs = ["checkSunday","fromSunday","untillSunday","checkMonday","fromMonday","untillMonday","checkTuesday","fromTuesday","untillTuesday","checkWednesday","fromWednesday","untillWednesday","checkThursday","fromThursday","untillThursday","checkFriday","fromFriday","untillFriday","checkSaturday","fromSaturday","untillSaturday"];
	var timeString = "";
	for (var i = 0; i < timeInputs.length; i += 3) {
		if (document.getElementById(timeInputs[i]).checked) {
			var ref1 = document.getElementById(timeInputs[i+1]);
				var ref2 = document.getElementById(timeInputs[i+2]);
			timeString += ((i/3)+"_")
			if (ref1.value == "") timeString += ("00:00-");
			else timeString += (document.getElementById(timeInputs[i+1]).value + "-");
				if (ref2.value == "") timeString += ("23:59 ");
			else timeString += (document.getElementById(timeInputs[i+2]).value + " ");
		}
	}
	timeString = timeString.slice(0, -1); 
	parseTimeData(timeString);
	return timeString;
}

function start() {
	websock = new WebSocket("ws://" + window.location.hostname + "/ws");
	websock.onopen = function(evt) {
		websock.send("{\"command\":\"picclist\"}");
	};
	websock.onclose = function(evt) {
		
	};
	websock.onerror = function(evt) {
		console.log(evt);
	};
	websock.onmessage = function(evt) {
		var obj = JSON.parse(evt.data);
		if (obj.command === "piccscan") {
			listSCAN(obj);
		} else if (obj.command === "picclist") {
			var node = document.getElementById("tablebody");
			while (node.hasChildNodes()) {
				node.removeChild(node.lastChild);
			}
			document.getElementById("loading-img").style.display = "none";
			listknownPICC(obj);
			sortTable();
			addRowHandlers();
		}
	};
}