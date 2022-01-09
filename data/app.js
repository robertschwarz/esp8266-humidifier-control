
const philips_hue_url = "YOUR_PHILIPS_HUE_URL";
const arduino_ip_address = "YOUR_ARDUINO_IP_ADDRESS";

let humidifier_controls = new Reef("#humidifier_controls", {
    data: {
        temperature: 23,
        humidity: 40,
        humidifier_status: true,
        auto_mode: true,
    },
    template: function (props) {
        return `
  <header>
    <h2>Temperature: ${props.temperature}°C<br />Humidity: ${props.humidity
            }%</h2>
  </header>
  <fieldset>
    <label for="humidifier_status">
      <input
        reef-checked="${props.humidifier_status}"
        onchange="toggle(this, 'humidifier_status')"
        type="checkbox"
        id="humidifier_status"
        name="humidifier_status"
        role="switch"
      />
      The humidifier is ${props.humidifier_status ? "on" : "off"}!
    </label>
    <label for="auto_mode">
      <input
        reef-checked="${props.auto_mode}"
        onchange="toggle(this, 'auto_mode')"
        type="checkbox"
        id="auto_mode"
        name="auto_mode"
        role="switch"
      />
      Automatic Mode is ${props.auto_mode ? "on" : "off"}!
    </label>
  </fieldset>`;
    },
});

let humidifier_range = new Reef("#humidifier_range", {
    data: {
        min_humidity: 45,
        max_humidity: 55,
        selected_min_humidity: 40,
        selected_max_humidity: 55,
    },
    template: function (props) {
        return `
    <header>
        <h2>Set a humidity range<br /><small>Current: ${props.min_humidity}% - ${props.max_humidity}%</small></h2>
      </header>
      <fieldset>
        <label for="min_humidity"
          >Minimum Humidity ${props.selected_min_humidity}%
          <input
            type="range"
            min="35"
            max="45"
            reef-value="${props.selected_min_humidity}"
            onchange="updateRange(this, 'selected_min_humidity')"
            id="min_humidity"
            name="min_humidity"
          />
        </label>
        <label for="max_humidity"
          >Maximum Humidity ${props.selected_max_humidity}%
          <input
            type="range"
            min="40"
            max="60"
            reef-value="${props.selected_max_humidity}"
            onchange="updateRange(this, 'selected_max_humidity')"
            id="max_humidity"
            name="max_humidity"
          />
        </label>
        <button id="submit">Save</button>
      </fieldset>
      `;
    },
});

const toggle = (elem, prop) => {
    humidifier_controls.data[prop] = elem.checked;
    if (prop === "auto_mode") {
        const params = `auto=${elem.checked ? 1 : 0}`;
        fetch(`${arduino_ip_address}/api/auto?${params}`, { method: "POST" })
            .then(() => {
                fetchHumidifierData();
                console.log(`Successfully toggled automatic mode on /api/auto! Status:`, elem.checked);
            })
            .catch((error) =>
                console.log(`An error occured while trying to post to /api/auto!`, error)
            );
    } else {
        const settings = {
            method: "PUT",
            body: JSON.stringify({ on: elem.checked }),
            headers: {
                "content-type": "application/json",
            },
        };
        fetch(`${philips_hue_url}/action`, settings)
            .then(() =>
                console.log(`Toggled the plug status! On Status:`, elem.checked)
            )
            .catch((error) =>
                console.log(`An error while trying to toggle the plug!`, error)
            );
    }
};

const postHumidifierStatus = (boolean) => {
    const params = `on=${boolean ? 1 : 0}`;
    fetch(`${arduino_ip_address}/api/status?${params}`, { method: "POST" })
        .then(() => console.log(`Successfully posted humidifier status to /api/status! On Status:`, boolean))
        .catch(error => console.log(`An error occured while trying to post to /api/status!`, error))
}

const updateRange = (elem, prop) => {
    humidifier_range.data[prop] = elem.value;
};

const fetchHumidifierData = () => {
    //arduino data
    fetch(`${arduino_ip_address}/api/metadata`)
        .then(response => response.json())
        .then(res_json => {
            humidifier_controls.data.temperature = res_json.temperature;
            humidifier_controls.data.humidity = res_json.humidity;
            humidifier_controls.data.auto_mode = Boolean(Number(res_json.autoMode));

            humidifier_range.data.min_humidity = res_json.minHumidity;
            humidifier_range.data.max_humidity = res_json.maxHumidity;
            console.log(`Successfully fetched Data from /api/metadata`, res_json);
        })
        .catch(error => console.log(`An error occured while trying to fetch data from /api/metadata!`, error))
    //philips hue data
    fetch(philips_hue_url)
        .then(response => response.json())
        .then(res_json => {
            humidifier_controls.data.humidifier_status = res_json.action.on;
            console.log(`Successfully fetched plug status from Philips! Status:`, res_json.action.on)
            postHumidifierStatus(res_json.action.on);
        })
        .catch(error => console.log(`Failed to fetch plug status from Philips!`, error))
};

const postRange = (min, max) => {
    const params = `min=${min}&max=${max}`;
    fetch(`${arduino_ip_address}/api/range?${params}`, { method: "POST" })
        .then(() => {
            console.log(`Successfully passed the humidity range to /api/range! Min: ${min}%, Max: ${max}%`)
            fetchHumidifierData()
        })
        .catch(error => console.log("An error occured while trying to post data to /api/range!", error))
};

fetchHumidifierData();
setInterval(fetchHumidifierData, 10000);

humidifier_controls.render();
humidifier_range.render();

document.getElementById("submit").addEventListener("click", () => {
    postRange(
        humidifier_range.data.selected_min_humidity,
        humidifier_range.data.selected_max_humidity
    );
});