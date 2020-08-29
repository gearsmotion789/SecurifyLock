// For full API documentation, including code examples, visit https://wix.to/94BuAAs
import * as functions from "backend/functions";

const hubName = "<Azure Sphere Hub Name>";

$w.onReady(function () {
});

export function lockId_change(event) {
	reloadDeviceDetails();
}

export function getCurrentStateBtn_click(event) {
	reloadDeviceDetails();
}

export function lockSw_change(event) {
	let checked = event.target.checked;
	let deviceId = $w('#lockId').options[$w('#lockId').selectedIndex].label;

	$w('#lockTxt').text = checked ? "Locked" : "Unlocked";

	if(deviceId.length > 0){
		functions.getToken()
			.then(token => {			
				functions.httpUpdate("https://"+hubName+".azure-devices.net/twins/"+deviceId+"?api-version=2020-05-31-preview",
						"patch",
						{
							"Authorization": token,
							"Content-Type": "application/json"
						},
						JSON.stringify({
							"properties": {
								"desired": {
									"locked": checked
								}
							}
						})
					)
					.then(data => {
						let locked = data.properties.desired.locked;
						console.log(`${deviceId} - Locked: ${locked}`);
					});
			});
	}
}

function reloadDeviceDetails(){
	let deviceId = $w('#lockId').options[$w('#lockId').selectedIndex].label;

	if(deviceId.length > 0){
		functions.getToken()
			.then(token => {
				
				functions.httpGet("https://"+hubName+".azure-devices.net/twins/"+deviceId+"?api-version=2020-05-31-preview",
						{
							"Authorization": token
						}
					)
					.then(data => {
						let connected = data.connectionState;
						let locked = data.properties.reported.locked;
						console.log(`${deviceId} - Connected: ${connected}`);
						console.log(`${deviceId} - Locked: ${locked}`);

						$w('#lockConnectionStatus').text = connected;
						$w('#packageReady').text = connected == "Connected" ? "Package ready for pickup" : "No package found";
						$w('#lockSw').checked = locked;
						$w('#lockTxt').text = locked ? "Locked" : "Unlocked";						
					});
			});
	}
}