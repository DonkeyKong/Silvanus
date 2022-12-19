function silvanusMain()
{
    getSettings();
    getStatus(true);
}

function getSettings()
{
    var url = "/system/settings";
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url);

    xhr.setRequestHeader("Accept", "application/json");
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
        if (xhr.status < 300 && xhr.status >= 200)
        {
            var settings = JSON.parse(xhr.responseText);
            document.getElementById("lightInterval").value = settings.lightInterval;
            document.getElementById("lightTime").value = settings.lightTime;
            document.getElementById("waterAmountPerDay").value = settings.waterAmountPerDay;
            document.getElementById("waterFlowRate").value = settings.waterFlowRate;
            document.getElementById("waterTime").value = settings.waterTime;
        }
        else
        {
            console.log(xhr.status);
            console.log(xhr.responseText);
        }
    }};

    xhr.send();
}

function getStatus(loop = true)
{
    var url = "/status";
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url);

    xhr.setRequestHeader("Accept", "application/json");
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
        if (xhr.status < 300 && xhr.status >= 200)
        {
            var status = JSON.parse(xhr.responseText);
            document.getElementById("statusTemp").innerText = status["temperature"];
            document.getElementById("statusMoisture").innerText = status["moisture"];
            document.getElementById("statusLight").innerText = status["light-on"] ? "On" : "Off";
            document.getElementById("statusPump").innerText = status["pump-on"] ? "On" : "Off";

            if (loop)
            {
                setTimeout(getStatus, 1000);
            }
        }
        else
        {
            console.log(xhr.status);
            console.log(xhr.responseText);
            document.getElementById("statusMsg").innerText = "Connection to Silvanus lost! Refresh to try again."
        }
    }};

    xhr.send();
}

function submitSettingsPatch()
{
    var url = "/system/settings";
    var xhr = new XMLHttpRequest();
    xhr.open("PATCH", url);

    xhr.setRequestHeader("Accept", "application/json");
    xhr.setRequestHeader("Content-Type", "application/json");

    xhr.onreadystatechange = function () {
    if (xhr.readyState === 4) {
        getSettings();
    }};

    var settings = 
    {
        "lightInterval": parseInt(document.getElementById("lightInterval").value),
        "lightTime": parseInt(document.getElementById("lightTime").value),
        "waterAmountPerDay": parseFloat(document.getElementById("waterAmountPerDay").value),
        "waterFlowRate": parseFloat(document.getElementById("waterFlowRate").value),
        "waterTime": parseInt(document.getElementById("waterTime").value)
    };

    var data = JSON.stringify(settings);

    xhr.send(data);
}

function submitSetLight()
{
    var lightMode = document.getElementById("lightMode").value;
    
    var xhr = new XMLHttpRequest();
    
    if (lightMode == "auto")
    {
        xhr.open("POST", "/auto-light");
        xhr.send();
    }
    else
    {
        xhr.open("PUT", "/light");
        xhr.setRequestHeader("Accept", "application/json");
        xhr.setRequestHeader("Content-Type", "application/json");
        xhr.send(lightMode == "on" ? "true" : "false");
    }
}