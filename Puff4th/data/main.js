//https://stackoverflow.com/questions/247483/http-get-request-in-javascript
function httpGetAsync(theUrl, callback)
{
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.onreadystatechange = function() { 
        if (xmlHttp.readyState == 4 && xmlHttp.status == 200)
            callback(xmlHttp.responseText);
    }
    xmlHttp.open("GET", theUrl, true); // true for asynchronous 
    xmlHttp.send(null);
}

function runFromVolume() {
    let volume = document.getElementById("volume").value
    let fudge = document.getElementById("fudge").value
    fudge = Math.floor(fudge * 1000)
    let runVolume = document.getElementById("runVolume").value
    let count = document.getElementById("count").value
    let speed = document.getElementById("speed").value
    let timePer = document.getElementById("timePer").value
    let timeBetween = document.getElementById("timeBetween").value
    timeBetween = Math.floor(timeBetween * 1000)
    let timeToDo = 0
    if (document.getElementById("doSpeed").checked) {
        if (!speed) {
            alert("You must input a value for the intake rate!")
            return
        }
        timeToDo = (runVolume / speed) * 60
    } else {
        if (!timePer) {
            alert("You must input a value for the time to fully intake!")
            return
        }
        timeToDo = timePer
    }
	 let peakHold = document.getElementById("peakHold").value
	 peakHold = Math.floor(peakHold * 1000)
	 let factor = document.getElementById("factor").value
    httpGetAsync(`/volumeRun?volume=${volume}&fudge=${fudge}&runVolume=${runVolume}&count=${count}&timeToDo=${timeToDo}&timeBetween=${timeBetween}&peakHold=${peakHold}&factor=${factor}`, (result) => {
        if (result == "Doing it") {
            alert("Success!")
        } else if (result == "Too fast") {
            alert("Speed is too fast!")
        } else if (result == "More time") {
            alert("Not enough time between runs!")
        }
    })
}

let file = null

function runFromCSV() {
    let reader = new FileReader()

    reader.onload = (evt) => {
        console.log(evt)
        let toSend = evt.target.result
        toSend = toSend.replace(/\r/g, "")
        toSend = toSend.replace(/\n/g, "_")
        let volume = document.getElementById("volume").value
        let fudge = document.getElementById("fudge").value
		  let factor = document.getElementById("factor").value
        httpGetAsync(`/csvRun?volume=${volume}&fudge=${fudge}&data=${toSend}&factor=${factor}`, (result) => {
            if (result == "Doing it") {
                alert("Success!")
            } else if (result == "Time error") {
                alert("The CSV file's time column is out of order!")
            } else if (result == "Too fast") {
                alert("Speed is too fast!")
            } else if (result == "Invalid format") {
                alert("The CSV file's format is invalid")
            }
        })
    }

    reader.readAsText(file)
}

function goToStart() {
    httpGetAsync(`/startRun`, (result) => {
        if (result == "Doing it") {
            alert("Success!")
        }
    })
}

function goToEnd() {
    httpGetAsync(`/endRun`, (result) => {
        if (result == "Doing it") {
            alert("Success!")
        }
    })
}

function killRuns() {
    httpGetAsync(`/killRuns`, (result) => {
        if (result == "Doing it") {
            alert("Success!")
        }
    })
}

function loadPrefs() {
	httpGetAsync(`/getPrefs`, (result) => {
		alert(result)
      let pairs = result.split("&")
		for (let i in pairs) {
			let pair = pairs[i].split("=")
			if (pair[0] == "volume") {
				document.getElementById("volume").value = pair[1]
			}
			if (pair[0] == "factor") {
				document.getElementById("factor").value = pair[1]
			}
			if (pair[0] == "delay") {
				document.getElementById("fudge").value = pair[1] / 1000
			}
		}
   })
}

document.getElementById("csvInput").addEventListener('change', (evt) => {
    console.log(evt)
    file = evt.target.files[0]
}, false)

loadPrefs()