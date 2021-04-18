document.getElementById("status").innerHTML = "WebSocket is not connected";

var websocket = new WebSocket('ws://'+location.hostname+'/');

console.log(location.hostname);

function sendText(text) {
  websocket.send(text);
}

function update(data) {
  var element = document.getElementById("pb");
  element.style.width = data + '%';
}

websocket.onmessage = function(evt) {
  document.getElementById("msg").innerHTML = evt.data;
  update(evt.data);
}

websocket.onopen = function(evt) {
  console.log('WebSocket connection opened');
  websocket.send("It's open! Hooray!!!");
  document.getElementById("status").innerHTML = "WebSocket is connected!";
}

websocket.onclose = function(evt) {
  console.log('Websocket connection closed');
  document.getElementById("status").innerHTML = "WebSocket closed";
}

websocket.onerror = function(evt) {
  console.log('Websocket error: ' + evt);
  document.getElementById("status").innerHTML = "WebSocket error!";
}
