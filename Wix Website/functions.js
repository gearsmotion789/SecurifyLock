import {fetch} from 'wix-fetch'

import HmacSHA256 from 'crypto-js/hmac-sha256';
import Base64 from 'crypto-js/enc-base64';

export function httpGet(url, headers){
	return new Promise((resolve, reject) => {
		fetch(url,
			{
				method: "get",
				headers: headers
			}
		)
		.then( (httpResponse) => {
			if (httpResponse.ok) {
				resolve(httpResponse.json());
			} else {
				reject(httpResponse.json());
			}
		})
		.catch(err => console.log(err));
	});
}

export function httpUpdate(url, method, headers, body){
	return new Promise((resolve, reject) => {
		fetch(url,
			{
				method: 'PUT',
				headers: headers,
				body: body
			}
		)
		.then( (httpResponse) => {
			if(httpResponse.ok) {
				resolve(httpResponse.json());
			} else {
				reject(httpResponse.json());
			}
		} )
		.catch(err => console.log(err));
	});
}

export function getToken(){
	// See this doc for details: https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-devguide-security
	var mhubName = "<Azure Sphere Hub Name>";
	var msigningKey = "<Azure Sphere Hub Signing Key>=";
	var mpolicyName = "<Azure Sphere Hub Policy Name>";
	var mexpiresInMins = 60;

	var generateSasToken = function(resourceUri, signingKey, policyName, expiresInMins) {
		resourceUri = encodeURIComponent(resourceUri);

		// Set expiration in seconds
		var expires = (Date.now() / 1000) + expiresInMins * 60;
		expires = Math.ceil(expires);
		var toSign = resourceUri + '\n' + expires;

		// Use crypto
		// var hmac = crypto.createHmac('sha256', Buffer.from(signingKey, 'base64'));
		// hmac.update(toSign);
		// var base64UriEncoded = encodeURIComponent(hmac.digest('base64'));

		var decodedKey = Base64.parse(signingKey); // The SHA256 key is the Base64 decoded version of the IoT Hub key
		var signature = HmacSHA256(toSign, decodedKey); // The signature generated from the decodedKey
		var base64UriEncoded = encodeURIComponent(Base64.stringify(signature)); // The url encoded version of the Base64 signature

		// Construct authorization string
		var token = "SharedAccessSignature sr=" + resourceUri + "&sig="
		+ base64UriEncoded + "&se=" + expires;
		if (policyName) token += "&skn="+policyName;
		return token;
	};

	var token = generateSasToken(
		mhubName + '.azure-devices.net',
		msigningKey,
		mpolicyName,
		mexpiresInMins
	);

	// console.log("Shared Access Signature:" + token);

	return token;
}